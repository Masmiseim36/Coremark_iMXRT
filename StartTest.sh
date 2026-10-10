#!/bin/bash
#
# StartTest.sh - automated CoreMark run for the iMXRT family
#
# For every selected project and every compiler profile this script
#   1. rebuilds the project with CrossBuild,
#   2. downloads it to the target with CrossLoad,
#   3. lets the benchmark run and waits until it has finished (wait_for_exit.js).
# At the end a summary of all runs is printed.
# The target prints the results of the benchmark via UART. With --uart they are captured while
# the benchmark runs and the score (CoreMark iterations per second) is shown in the summary. The
# complete UART output of every run is saved in out/uart/.
#
# Dual core devices (iMXRT1160 / iMXRT1170):
#   The Cortex-M4 project is listed in "debug_dependent_projects" ("Debug Additional Projects") of
#   the Cortex-M7 project. CrossLoad downloads both images with a single call, and wait_for_exit.js
#   starts both cores and waits for the end of the benchmark on each of them. Every core appears
#   as a separate line in the summary.
#
# Requires a CrossWorks version with multi-core support in CrossLoad (script method
# Debug.getdebugprojects(), CrossWorks 5.5 nightly builds).
# Run "./StartTest.sh --help" for the usage.

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1
ScriptDir="$PWD"

# Compiler profiles: label | gcc_optimization_level | link_time_optimization | marker printed by the target
# The marker is passed as GCC_OPTIONS to the compiler. It must not contain blanks.
Profiles=(
	"O3 LTO|Level 3|Yes|###_O3_LTO_###"
	"O3|Level 3|No|###_O3_###"
	"O2|Level 2|No|###_O2_###"
	"O1|Level 1|No|###_O1_###"
	"O0|Level 0|No|###_O0_###"
	"OG|Debug|No|###_Debug_###"
	"OSize|Optimize For Size|No|###_Optimize_For_Size_###"
)

ProfileLabels=""
for Profile in "${Profiles[@]}"; do
	ProfileLabels+="${ProfileLabels:+,}${Profile%%|*}"
done

# ---------------------------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------------------------
Usage ()
{
	cat <<EOF
Usage: $(basename "$0") [options] [project ... | all]

Builds the given projects with every compiler profile, downloads them to the target and waits
until the benchmark has finished. Without a project name an interactive selection is shown.

Options:
  -s, --solution FILE        solution to use                        (default: Coremark_Extended.hzp)
  -C, --crossworks-dir DIR   CrossWorks installation directory      (default: see below)
  -i, --interface NAME       CrossLoad target interface             (default: CMSIS-DAP)
  -p, --probe SERIAL         serial number of the CMSIS-DAP probe   (default: first probe found)
  -c, --config NAME          build configuration                    (default: Release)
  -t, --timeout SECONDS      safety limit per run (all cores); the end of the benchmark is
                             detected by a breakpoint, this only stops a hung target
                                                                    (default: 600)
  -P, --profiles LIST        comma separated profile labels         (default: all)
                             available: $ProfileLabels
  -u, --uart PORT            serial port of the target console, e.g. COM3 or /dev/ttyACM0;
                             the benchmark results are captured and shown in the summary
                                                                    (default: no capture)
  -b, --build-only           build only, do not touch the target
  -e, --stop-on-error        stop at the first failing run          (default: continue)
  -n, --dry-run              only print the commands
  -l, --list                 list the projects and exit
  -h, --help                 show this help

Options with a value can be given as "--timeout 300" or "--timeout=300".

Default CrossWorks directory: C:/Program Files/Rowley CrossWorks for ARM 5.5 (Windows),
/opt/rowley/crossworks_for_arm_5.5 (Linux).

Examples:
  $(basename "$0") Coremark_1060
  $(basename "$0") -P "O3,O2" -p 000123456789 Coremark_1160CM7 Coremark_1060
  $(basename "$0") --uart COM3 -P O3 Coremark_1060
  $(basename "$0") --build-only all
EOF
}

Fail ()
{
	echo "Error: $1" >&2
	echo "Use --help for the usage." >&2
	exit 1
}

case "$(uname -s)" in
	Linux*)
		CrossWorksDir="/opt/rowley/crossworks_for_arm_5.5"
		ExeSuffix=""
		;;
	*)	# Windows (Git Bash, MSYS, Cygwin)
		CrossWorksDir="C:/Program Files/Rowley CrossWorks for ARM 5.5"
		ExeSuffix=".exe"
		;;
esac

SolutionFile="Coremark_Extended.hzp"
TargetInterface="CMSIS-DAP"
ProbeSerial=""
BuildConfig="Release"
WaitTimeoutSec=600
ProfileFilter=""
SkipLoad=0
StopOnError=0
DryRun=0
ListOnly=0
LoadAttempts=3
UartPort=""
UartBaud=115200
Arguments=()

while [ $# -gt 0 ]; do
	Option="$1"
	Value=""
	HasValue=0
	case "$Option" in
		--*=*)
			Value="${Option#*=}"
			Option="${Option%%=*}"
			HasValue=1
			;;
	esac

	# Options that need a value
	case "$Option" in
		-s|--solution|-C|--crossworks-dir|-i|--interface|-p|--probe|-c|--config|-t|--timeout|-P|--profiles|-u|--uart)
			if [ $HasValue -eq 0 ]; then
				[ $# -ge 2 ] || Fail "Option '$Option' needs a value."
				Value="$2"
				shift
			fi
			;;
	esac

	case "$Option" in
		-s|--solution)         SolutionFile="$Value" ;;
		-C|--crossworks-dir)   CrossWorksDir="$Value" ;;
		-i|--interface)        TargetInterface="$Value" ;;
		-p|--probe)            ProbeSerial="$Value" ;;
		-c|--config)           BuildConfig="$Value" ;;
		-t|--timeout)
			[[ "$Value" =~ ^[0-9]+$ ]] && [ "$Value" -gt 0 ] || Fail "Invalid timeout '$Value'."
			WaitTimeoutSec="$Value"
			;;
		-P|--profiles)         ProfileFilter="$Value" ;;
		-u|--uart)             UartPort="$Value" ;;
		-b|--build-only)       SkipLoad=1 ;;
		-e|--stop-on-error)    StopOnError=1 ;;
		-n|--dry-run)          DryRun=1 ;;
		-l|--list)             ListOnly=1 ;;
		-h|--help)             Usage; exit 0 ;;
		--)                    shift; Arguments+=("$@"); break ;;
		-*)                    Fail "Unknown option '$Option'." ;;
		*)                     Arguments+=("$Option") ;;
	esac
	shift
done

# Check the profile filter
if [ -n "$ProfileFilter" ]; then
	IFS=',' read -r -a FilterItems <<< "$ProfileFilter"
	for FilterItem in "${FilterItems[@]}"; do
		[[ ",$ProfileLabels," == *",$FilterItem,"* ]] || Fail "Unknown profile '$FilterItem'. Available: $ProfileLabels"
	done
fi

# ---------------------------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------------------------
Crossbuild="$CrossWorksDir/bin/crossbuild$ExeSuffix"
Crossload="$CrossWorksDir/bin/crossload$ExeSuffix"

if command -v cygpath >/dev/null 2>&1; then
	ToWindowsPath () { cygpath -w "$1"; }
	ToMixedPath ()   { cygpath -m "$1"; }
else
	ToWindowsPath () { printf '%s\n' "$1"; }
	ToMixedPath ()   { printf '%s\n' "$1"; }
fi

WaitScript="$ScriptDir/wait_for_exit.js"
RunScript="$ScriptDir/out/wait_for_exit_run.js"      # generated, contains the parameters
ResultFile="$ScriptDir/out/wait_for_exit_result.txt" # written by the script, read by this script

if [ "$DryRun" != 1 ] && [ "$ListOnly" != 1 ]; then
	for Tool in "$Crossbuild" "$Crossload"; do
		[ -f "$Tool" ] || Fail "'$Tool' not found. Use --crossworks-dir to set the CrossWorks installation directory."
	done
	grep -aq 'getdebugprojects' "$Crossload" || Fail "'$Crossload' has no multi-core support (Debug.getdebugprojects()). Use a newer CrossWorks."
fi
for File in "$SolutionFile" "$WaitScript"; do
	[ -f "$File" ] || Fail "'$File' not found."
done

# Serial port: under Windows (Git Bash / MSYS) COMn is the device /dev/ttyS(n-1)
UartDevice="$UartPort"
if [[ "$UartPort" =~ ^[Cc][Oo][Mm]([0-9]+)$ ]]; then
	UartDevice="/dev/ttyS$((BASH_REMATCH[1] - 1))"
fi
if [ -n "$UartDevice" ] && [ "$SkipLoad" != 1 ] && [ "$DryRun" != 1 ]; then
	[ -c "$UartDevice" ] || Fail "Serial port '$UartPort' ($UartDevice) not found."
fi

# Print (and, unless --dry-run is given, execute) a command
Run ()
{
	printf '>'
	printf ' %q' "$@"
	printf '\n'
	[ "$DryRun" = 1 ] && return 0
	"$@"
}

# ---------------------------------------------------------------------------------------------
# Read the projects from the solution file
# ---------------------------------------------------------------------------------------------
# A project that is listed in the debug_dependent_projects of another project is the secondary core
# of a dual core device. It is not selected on its own, but run together with its main project.
# Projects which are only listed in project_dependencies (build order) are still selectable.
# The awk script prints one line "<main project>|<secondary projects separated by ;>" per main project.
BuildProjects=()
declare -A Secondaries

while IFS='|' read -r Name Secondary; do
	BuildProjects+=("$Name")
	Secondaries["$Name"]="$Secondary"
done < <(awk '
	{ gsub(/\r/, "") }
	/<project[ \t]+Name="/ {
		Project = $0
		sub(/^.*<project[ \t]+Name="/, "", Project)
		sub(/".*$/, "", Project)
		Names[++Count] = Project
		next
	}
	/<\/project>/ { Project = "" }
	Project != "" && /debug_dependent_projects="[^"]+"/ {
		Line = $0
		sub(/^.*debug_dependent_projects="/, "", Line)
		sub(/".*$/, "", Line)
		gsub(/[ \t]/, "", Line)
		Dependent[Project] = Line
		Items = split(Line, Child, ";")
		for (Item = 1; Item <= Items; Item++) IsSecondary[Child[Item]] = 1
	}
	END {
		for (Index = 1; Index <= Count; Index++)
			if (!(Names[Index] in IsSecondary)) print Names[Index] "|" Dependent[Names[Index]]
	}
' "$SolutionFile")

[ ${#BuildProjects[@]} -gt 0 ] || Fail "No projects found in $SolutionFile."

echo "Main projects:"
for ProjectName in "${BuildProjects[@]}"; do
	echo "- $ProjectName${Secondaries[$ProjectName]:+ (+ secondary core: ${Secondaries[$ProjectName]//;/ })}"
done
[ "$ListOnly" = 1 ] && exit 0

# ---------------------------------------------------------------------------------------------
# Select the projects
# ---------------------------------------------------------------------------------------------
SelectedProjects=()

if [ ${#Arguments[@]} -gt 0 ]; then
	for Argument in "${Arguments[@]}"; do
		if [ "$Argument" = "all" ]; then
			SelectedProjects=("${BuildProjects[@]}")
			break
		fi
		[[ " ${BuildProjects[*]} " == *" $Argument "* ]] || Fail "'$Argument' is not a main project of $SolutionFile."
		SelectedProjects+=("$Argument")
	done
else
	echo ""
	echo "Please select the projects to run:"
	echo "0) All main projects"
	for Index in "${!BuildProjects[@]}"; do
		echo "$((Index + 1))) ${BuildProjects[$Index]}"
	done

	read -r -p "Selection (numbers separated by blanks or commas): " UserSelection || exit 1
	for Number in ${UserSelection//,/ }; do
		if [[ "$Number" == 0 ]]; then
			SelectedProjects=("${BuildProjects[@]}")
			break
		elif [[ "$Number" =~ ^[0-9]+$ ]] && [ "$Number" -ge 1 ] && [ "$Number" -le ${#BuildProjects[@]} ]; then
			SelectedProjects+=("${BuildProjects[$((Number - 1))]}")
		else
			Fail "Invalid selection: '$Number'"
		fi
	done
fi

[ ${#SelectedProjects[@]} -gt 0 ] || Fail "Nothing selected."

# ---------------------------------------------------------------------------------------------
# Build, load and wait
# ---------------------------------------------------------------------------------------------
ResultNames=()
ResultStates=()
ResultSeconds=()
ResultScores=()

PrintSummary ()
{
	local Index Failed=0
	echo ""
	echo "================================ Summary ================================"
	for Index in "${!ResultNames[@]}"; do
		printf '%-42s %-28s %5s s  %s\n' "${ResultNames[$Index]}" "${ResultStates[$Index]}" "${ResultSeconds[$Index]}" "${ResultScores[$Index]}"
		case "${ResultStates[$Index]}" in
			FINISHED|BUILT|"DRY RUN") ;;
			*) Failed=1 ;;
		esac
	done
	echo "========================================================================="
	return $Failed
}

trap 'echo; echo "Interrupted."; PrintSummary; exit 130' INT

# RecordResult <run name> <state> [score]
# Stores and prints the result of a run. Exits if --stop-on-error is given and the run has failed.
RecordResult ()
{
	ResultNames+=("$1")
	ResultStates+=("$2")
	ResultSeconds+=("$((SECONDS - StartSeconds))")
	ResultScores+=("$3")
	echo "$1: $2"
	StartSeconds=$SECONDS

	case "$2" in
		FINISHED|BUILT|"DRY RUN") ;;
		*)	if [ "$StopOnError" = 1 ]; then
				PrintSummary
				exit 1
			fi
			;;
	esac
}

# ---------------------------------------------------------------------------------------------
# UART capture
# ---------------------------------------------------------------------------------------------
# The target prints the result of every core as a block of lines, which ends with "Coremark done -
# M7" (or M4, M33). Both cores of a dual core device use the same UART, their blocks follow one
# another. The cores are told apart by this name: the suffix CM<n> of the project name gives the
# core (Coremark_1170CM4 -> M4), a project without this suffix runs on a Cortex-M7.
declare -A UartScores
UartPid=""
UartLog=""

# StartUart <log file>
# Starts to write everything which the target sends to the log file. The serial port is opened by
# the shell (file descriptor 9) and stays open, because the port settings are lost when it is closed.
StartUart ()
{
	[ -n "$UartDevice" ] && [ "$DryRun" != 1 ] || return 0
	UartLog="$1"
	mkdir -p "$(dirname "$UartLog")"
	if ! exec 9< "$UartDevice"; then
		echo "Warning: cannot open $UartDevice, the results are not captured."
		UartDevice=""
		return 0
	fi
	stty "$UartBaud" cs8 -parenb -cstopb -echo -icanon -ixon -ixoff -icrnl -inlcr -opost clocal min 1 time 0 <&9
	cat <&9 >> "$UartLog" &
	UartPid=$!
	# Discard what the driver had buffered before the port was opened
	sleep 1
	: > "$UartLog"
}

# StopUart: ends the capture and reads the scores from the log file into UartScores
StopUart ()
{
	[ -n "$UartPid" ] || return 0
	sleep 1		# the last characters of the target may still be on their way
	kill "$UartPid"
	wait "$UartPid" 2>/dev/null
	exec 9<&-
	UartPid=""

	local Core Rate Valid
	UartScores=()
	while IFS='|' read -r Core Rate Valid; do
		if [ -z "$Rate" ]; then
			UartScores[$Core]="no result"
		elif [ -z "$Valid" ]; then
			UartScores[$Core]="$Rate it/s (NOT VALIDATED)"
		else
			UartScores[$Core]="$Rate it/s"
		fi
	done < <(tr -d '\r' < "$UartLog" | awk '
		/CoreMark Size/                { Rate = ""; Valid = "" }
		/^Iterations\/Sec/             { Rate = sprintf("%.2f", $3) }
		/Correct operation validated/  { Valid = "yes" }
		/^Coremark done - /            { print $NF "|" Rate "|" Valid }
	')
}

# UartScoreOf <project>: the score of the core of the project, empty if the UART is not captured
UartScoreOf ()
{
	local Core="M7"
	[[ "$1" =~ CM([0-9]+)$ ]] && Core="M${BASH_REMATCH[1]}"
	[ -n "$UartDevice" ] && [ -n "$UartLog" ] && echo "${UartScores[$Core]:-no result}"
}
# BuildProject <project> <optimization level> <link time optimization> <marker>
# All properties are set as solution properties (-sproperty). A project property (-property) would
# be overruled by the settings of the "Release" solution configuration and silently ignored.
BuildProject ()
{
	Run "$Crossbuild" \
		-verbose \
		-config "$BuildConfig" \
		-project "$1" \
		-sproperty "c_preprocessor_definitions=NDEBUG;GCC_OPTIONS=\"$4\"" \
		-sproperty "gcc_optimization_level=$2" \
		-sproperty "link_time_optimization=$3" \
		-rebuild \
		-echo \
		"$SolutionFile"
}

# LoadAndWait <project> <label>
# Downloads the project (and its secondary core), runs all cores, waits until the benchmark has
# finished on every core and records the result of each core.
LoadAndWait ()
{
	local Project="$1" Label="$2" Attempt Name State
	local CrossloadArgs=(-target "$TargetInterface")
	[ -n "$ProbeSerial" ] && CrossloadArgs+=(-setprop "Use Serial Number=$ProbeSerial")
	CrossloadArgs+=(-solution "$SolutionFile" -project "$Project" -config "$BuildConfig")

	# The script gets its parameters through a generated header.
	mkdir -p "$(dirname "$RunScript")"
	rm -f "$ResultFile"
	{
		printf 'var WaitTimeoutMs = %d;\n' $((WaitTimeoutSec * 1000))
		printf 'var ResultFile = "%s";\n'  "$(ToMixedPath "$ResultFile")"
		cat "$WaitScript"
	} > "$RunScript"

	echo "Downloading $Project and waiting for the benchmark to finish"
	StartUart "$ScriptDir/out/uart/${Project}_${Label// /_}.log"
	# stdin is closed, so that CrossLoad cannot get stuck at its interactive prompt if the script fails.
	# "-break main" lets CrossLoad run the application up to main(), which gives the script a defined start.
	# CrossLoad sometimes fails while it connects to the target (e.g. "Write word to address ...
	# timed out" when the core is reset). This happens before the script is started, so the call is
	# simply repeated.
	for ((Attempt = 1; Attempt <= LoadAttempts; Attempt++)); do
		[ $Attempt -gt 1 ] && echo "CrossLoad failed, trying again (attempt $Attempt of $LoadAttempts)"
		Run "$Crossload" "${CrossloadArgs[@]}" -debug -break main -script "$(ToWindowsPath "$RunScript")" < /dev/null && break
	done
	StopUart
	if [ $Attempt -gt $LoadAttempts ]; then
		RecordResult "$Project [$Label]" "LOAD FAILED"
	elif [ "$DryRun" = 1 ]; then
		RecordResult "$Project [$Label]" "DRY RUN"
	elif [ ! -s "$ResultFile" ]; then
		RecordResult "$Project [$Label]" "NO RESULT"
	else
		# The script writes one line "<debug project>|<state>" per core
		while IFS='|' read -r Name State; do
			RecordResult "${Name%_$BuildConfig} [$Label]" "${State%$'\r'}" "$(UartScoreOf "${Name%_$BuildConfig}")"
		done < "$ResultFile"
	fi
}

for Project in "${SelectedProjects[@]}"; do
	for Profile in "${Profiles[@]}"; do
		IFS='|' read -r Label Level Lto Marker <<< "$Profile"
		if [ -n "$ProfileFilter" ] && [[ ",$ProfileFilter," != *",$Label,"* ]]; then
			continue
		fi
		RunName="$Project [$Label]"
		StartSeconds=$SECONDS

		echo ""
		echo "=============================================================================="
		echo "$RunName: $Level, link time optimization: $Lto"
		echo "=============================================================================="

		# Building the main project also builds the secondary core project (project_dependencies).
		if ! BuildProject "$Project" "$Level" "$Lto" "$Marker"; then
			RecordResult "$RunName" "BUILD FAILED"
		elif [ "$SkipLoad" = 1 ]; then
			RecordResult "$RunName" "BUILT"
		else
			LoadAndWait "$Project" "$Label"
		fi
	done
done

PrintSummary
