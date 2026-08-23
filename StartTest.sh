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
# Dual core devices (iMXRT1160 / 1170 / 1180 / 1186):
#   The secondary core is listed in "debug_dependent_projects" ("Debug Additional Projects") of
#   the main core. CrossLoad downloads both images with a single call, and wait_for_exit.js
#   starts both cores and waits for the end of the benchmark on each of them. Every core appears
#   as a separate row in the summary.
#
# Requires a CrossWorks version with multi-core support in CrossLoad (script method
# Debug.getdebugprojects(), CrossWorks 5.5 nightly builds).
# Run "./StartTest.sh --help" for the usage.

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1
ScriptDir="$PWD"

# Compiler profiles: label | gcc_optimization_level | link_time_optimization | marker printed by the target
# The marker is passed as GCC_OPTIONS to the compiler. It must not contain blanks.
# Column order of the summary table. "O3 LTO" is shown as "O3 + LTO".
Profiles=(
	"O0|Level 0|No|###_O0_###"
	"OG|Debug|No|###_Debug_###"
	"O1|Level 1|No|###_O1_###"
	"OSize|Optimize For Size|No|###_Optimize_For_Size_###"
	"OSize LTO|Optimize For Size|Yes|###_Optimize_For_Size_LTO_###"
	"O2|Level 2|No|###_O2_###"
	"O2 LTO|Level 2|Yes|###_O2_LTO_###"
	"O3|Level 3|No|###_O3_###"
	"O3 LTO|Level 3|Yes|###_O3_LTO_###"
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
  $(basename "$0") iMXRT1060-EVK
  $(basename "$0") -P "O3,O2" -p 000123456789 "iMXRT1160-EVK CM7" iMXRT1060-EVK
  $(basename "$0") --uart COM3 -P O3 iMXRT1060-EVK
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

SelectedProfiles=()
if [ -n "$ProfileFilter" ]; then
	IFS=',' read -r -a FilterItems <<< "$ProfileFilter"
	for FilterItem in "${FilterItems[@]}"; do
		[[ ",$ProfileLabels," == *",$FilterItem,"* ]] || Fail "Unknown profile '$FilterItem'. Available: $ProfileLabels"
	done
	for Profile in "${Profiles[@]}"; do
		[[ ",$ProfileFilter," == *",${Profile%%|*},"* ]] && SelectedProfiles+=("$Profile")
	done
else
	SelectedProfiles=("${Profiles[@]}")
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
		Items = split(Line, Child, ";")
		Joined = ""
		for (Item = 1; Item <= Items; Item++) {
			sub(/^[ \t]+/, "", Child[Item])
			sub(/[ \t]+$/, "", Child[Item])
			if (Child[Item] == "") continue
			IsSecondary[Child[Item]] = 1
			Joined = Joined (Joined == "" ? "" : ";") Child[Item]
		}
		Dependent[Project] = Joined
	}
	END {
		for (Index = 1; Index <= Count; Index++)
			if (!(Names[Index] in IsSecondary)) print Names[Index] "|" Dependent[Names[Index]]
	}
' "$SolutionFile")

[ ${#BuildProjects[@]} -gt 0 ] || Fail "No projects found in $SolutionFile."

# ---------------------------------------------------------------------------------------------
# Select the projects
# ---------------------------------------------------------------------------------------------
ProjectLabel ()
{
	echo "$1${Secondaries[$1]:+ (+ secondary core: ${Secondaries[$1]//;/ })}"
}

if [ "$ListOnly" = 1 ]; then
	for ProjectName in "${BuildProjects[@]}"; do
		ProjectLabel "$ProjectName"
	done
	exit 0
fi

IsMainProject ()
{
	local Candidate
	for Candidate in "${BuildProjects[@]}"; do
		[ "$Candidate" = "$1" ] && return 0
	done
	return 1
}

SelectedProjects=()

if [ ${#Arguments[@]} -gt 0 ]; then
	for Argument in "${Arguments[@]}"; do
		if [ "$Argument" = "all" ]; then
			SelectedProjects=("${BuildProjects[@]}")
			break
		fi
		IsMainProject "$Argument" || Fail "'$Argument' is not a main project of $SolutionFile."
		SelectedProjects+=("$Argument")
	done
else
	echo "Please select the projects to run:"
	echo "0) All main projects"
	for Index in "${!BuildProjects[@]}"; do
		echo "$((Index + 1))) $(ProjectLabel "${BuildProjects[$Index]}")"
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
ResultProjects=()
ResultLabels=()
ResultStates=()
ResultScores=()
ResultSizes=()

# ControllerTitle <project>: iMXRT1060-EVK -> **iMX RT1060**, iMXRT1170-EVK CM7 -> **iMX RT1170_CM7**
ControllerTitle ()
{
	local Name="$1"
	Name="${Name/-EVK/}"
	Name="${Name/iMXRT/iMX RT}"
	if [[ "$Name" =~ ^(.*)[[:space:]]CM([0-9]+)$ ]]; then
		Name="${BASH_REMATCH[1]}_CM${BASH_REMATCH[2]}"
	fi
	printf '**%s**' "$Name"
}

# TableCell <state> <score> <bytes>: "430.50 it/s" + 22120 -> "430.50<br>22,120"
TableCell ()
{
	local State="$1" Score="$2" Bytes="$3" Upper="-" Lower="" Rate="" n out
	if [[ "$Bytes" =~ ^[0-9]+$ ]]; then
		n=$Bytes
		while [ ${#n} -gt 3 ]; do
			out=",${n: -3}$out"
			n="${n:0:${#n}-3}"
		done
		Lower="$n$out"
	fi
	if [[ "$Score" =~ ([0-9]+\.[0-9]+) ]]; then
		Rate="${BASH_REMATCH[1]}"
		[[ "$Score" == *NOT* ]] && Rate="${Rate}*"
	fi
	case "$State" in
		FINISHED)           Upper="${Rate:--}" ;;
		BUILT|"DRY RUN"|"") Upper="-" ;;
		*)                  Upper="$State" ;;
	esac
	if [ -n "$Lower" ]; then
		printf '%s<br>%s' "$Upper" "$Lower"
	else
		printf '%s' "$Upper"
	fi
}

# Markdown table: one row per core, columns in the order of SelectedProfiles.
# Each cell is "score<br>size" so the block can be pasted into a Markdown table.
PrintSummary ()
{
	local Index Failed=0 Project Label Profile
	local -a Projects
	local -A Seen CellText

	for Index in "${!ResultProjects[@]}"; do
		Project="${ResultProjects[$Index]}"
		Label="${ResultLabels[$Index]}"
		if [ -z "${Seen[$Project]}" ]; then
			Projects+=("$Project")
			Seen[$Project]=1
		fi
		CellText["$Project|$Label"]=$(TableCell "${ResultStates[$Index]}" "${ResultScores[$Index]}" "${ResultSizes[$Index]}")
		case "${ResultStates[$Index]}" in
			FINISHED|BUILT|"DRY RUN") ;;
			*) Failed=1 ;;
		esac
	done

	if [ ${#Projects[@]} -eq 0 ]; then
		echo ""
		echo "No results."
		return $Failed
	fi

	echo ""
	printf 'Controller'
	for Profile in "${SelectedProfiles[@]}"; do
		Label="${Profile%%|*}"
		printf ' | %s' "${Label/ LTO/ + LTO}"
	done
	printf '\n--------'
	for Profile in "${SelectedProfiles[@]}"; do
		printf ' | --------:'
	done
	printf '\n'
	for Project in "${Projects[@]}"; do
		printf '%s' "$(ControllerTitle "$Project")"
		for Profile in "${SelectedProfiles[@]}"; do
			Label="${Profile%%|*}"
			printf ' | %s' "${CellText["$Project|$Label"]}"
		done
		printf '\n'
	done
	echo ""
	return $Failed
}

trap 'echo; echo "Interrupted."; PrintSummary; exit 130' INT

# RecordResult <project> <label> <state> [score] [size in bytes]
# Stores and prints the result of a run. Exits if --stop-on-error is given and the run has failed.
RecordResult ()
{
	ResultProjects+=("$1")
	ResultLabels+=("$2")
	ResultStates+=("$3")
	ResultScores+=("$4")
	ResultSizes+=("$5")
	echo "$1 [$2]: $3${5:+  .fast $5 B}${4:+  $4}"

	case "$3" in
		FINISHED|BUILT|"DRY RUN") ;;
		*)	if [ "$StopOnError" = 1 ]; then
				PrintSummary
				exit 1
			fi
			;;
	esac
}

# RecordCore <project> <label> <state> [score]
# Like RecordResult, and fills in the .fast size from the map.
RecordCore ()
{
	RecordResult "$1" "$2" "$3" "$4" "$(MapSectionSize "$1" ".fast")"
}

# RecordBuilt <main project> <label>
# Records the main project and its secondary core as BUILT (--build-only).
RecordBuilt ()
{
	local Project="$1" Label="$2" Core
	local -a Cores
	RecordCore "$Project" "$Label" "BUILT"
	IFS=';' read -r -a Cores <<< "${Secondaries[$Project]}"
	for Core in "${Cores[@]}"; do
		[ -n "$Core" ] && RecordCore "$Core" "$Label" "BUILT"
	done
}

# ---------------------------------------------------------------------------------------------
# UART capture
# ---------------------------------------------------------------------------------------------
# The target prints the result of every core as a block of lines, which ends with "Coremark done -
# M7" (or M4, M33). Both cores of a dual core device use the same UART, their blocks follow one
# another. The cores are told apart by this name: the suffix CM<n> of the project name gives the
# core (iMXRT1170-EVK CM4 -> M4), a project without this suffix runs on a Cortex-M7.
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

# MapSectionSize <project> <section>
# Bytes of the output section, from the GNU ld map line
#   .fast  0x00000400  0x30e8 load address 0x...
# That length ($3) is the number CrossWorks shows. Do not use:
#   - the first ".<section>" line (LTO discarded input, size 0)
#   - later SIZEOF expressions (__fast_end__ = start + SIZEOF(.fast), …)
#   - input sections named .fast.<function>
MapSectionSize ()
{
	local Map="$ScriptDir/bin/$1 $BuildConfig/$1.map" Hex
	[ -f "$Map" ] && [ "$DryRun" != 1 ] || return 0
	Hex=$(tr -d '\r' < "$Map" | awk -v name="$2" '
		$1 == name && tolower($4) == "load" && tolower($5) == "address" {
			print $3
			exit
		}
	')
	[[ "$Hex" =~ ^0x[0-9A-Fa-f]+$ ]] || return 0
	printf '%d' $((Hex))
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
	local Project="$1" Label="$2" Attempt Name State CoreProject
	rm -f "$ResultFile"

	echo "Downloading $Project and waiting for the benchmark to finish"
	StartUart "$ScriptDir/out/uart/${Project}_${Label// /_}.log"
	# stdin is closed, so that CrossLoad cannot get stuck at its interactive prompt if the script fails.
	# "-break main" lets CrossLoad run the application up to main(), which gives the script a defined start.
	# CrossLoad sometimes fails while it connects to the target (e.g. "Write word to address ...
	# timed out" when the core is reset). This happens before the script is started, so the call is
	# simply repeated.
	for ((Attempt = 1; Attempt <= LoadAttempts; Attempt++)); do
		[ $Attempt -gt 1 ] && echo "CrossLoad failed, trying again (attempt $Attempt of $LoadAttempts)"
		Run "$Crossload" "${CrossloadBase[@]}" -project "$Project" -debug -break main -script "$(ToWindowsPath "$RunScript")" < /dev/null && break
	done
	StopUart
	if [ $Attempt -gt $LoadAttempts ]; then
		RecordCore "$Project" "$Label" "LOAD FAILED"
	elif [ "$DryRun" = 1 ]; then
		RecordResult "$Project" "$Label" "DRY RUN"
	elif [ ! -s "$ResultFile" ]; then
		RecordCore "$Project" "$Label" "NO RESULT"
	else
		# The script writes one line "<debug project>|<state>" per core
		while IFS='|' read -r Name State; do
			CoreProject="${Name%_$BuildConfig}"
			RecordCore "$CoreProject" "$Label" "${State%$'\r'}" "$(UartScoreOf "$CoreProject")"
		done < "$ResultFile"
	fi
}

if [ "$SkipLoad" != 1 ]; then
	mkdir -p "$(dirname "$RunScript")"
	{
		printf 'var WaitTimeoutMs = %d;\n' $((WaitTimeoutSec * 1000))
		printf 'var ResultFile = "%s";\n'  "$(ToMixedPath "$ResultFile")"
		cat "$WaitScript"
	} > "$RunScript"
	CrossloadBase=(-target "$TargetInterface")
	[ -n "$ProbeSerial" ] && CrossloadBase+=(-setprop "Use Serial Number=$ProbeSerial")
	CrossloadBase+=(-solution "$SolutionFile" -config "$BuildConfig")
fi

for Project in "${SelectedProjects[@]}"; do
	for Profile in "${SelectedProfiles[@]}"; do
		IFS='|' read -r Label Level Lto Marker <<< "$Profile"

		echo ""
		echo "=============================================================================="
		echo "$Project [$Label]: $Level, link time optimization: $Lto"
		echo "=============================================================================="

		# Building the main project also builds the secondary core project (project_dependencies).
		if ! BuildProject "$Project" "$Level" "$Lto" "$Marker"; then
			RecordResult "$Project" "$Label" "BUILD FAILED"
		elif [ "$SkipLoad" = 1 ]; then
			RecordBuilt "$Project" "$Label"
		else
			LoadAndWait "$Project" "$Label"
		fi
	done
done

PrintSummary
