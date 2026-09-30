#!/bin/bash
#
# StartTest.sh - automated CoreMark run for the iMXRT family
#
# For every selected project and every compiler profile this script
#   1. rebuilds the project with CrossBuild,
#   2. downloads it to the target with CrossLoad,
#   3. lets the benchmark run and waits until it has finished (wait_for_exit.js).
# At the end a summary of all runs is printed.
# The results of the benchmark itself are printed by the target via UART. They are not
# captured by this script.
#
# Dual core devices (iMXRT1160 / iMXRT1170):
#   The Cortex-M4 project is listed in "debug_dependent_projects" of the Cortex-M7 project. So
#   CrossLoad downloads both cores when the M7 project is started, and the M7 application starts the
#   M4 core (see portable_init in core_portme.cpp). Only the M7 project is therefore selected here.
#   The debug session observes only the M7 core, so after the M7 has finished the script waits
#   --secondary-wait seconds longer for the M4.
#
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
  -t, --timeout SECONDS      max. time to wait for the benchmark    (default: 600)
  -w, --secondary-wait SEC   extra wait time for a secondary core   (default: 20, 0 = none)
  -P, --profiles LIST        comma separated profile labels         (default: all)
                             available: $ProfileLabels
  -b, --build-only           build only, do not touch the target
  -e, --stop-on-error        stop at the first failing run          (default: continue)
  -n, --dry-run              only print the commands
  -l, --list                 list the projects and exit
  -h, --help                 show this help

Options with a value can be given as "--timeout 300" or "--timeout=300".

Default CrossWorks directory: C:/Program Files/Rowley CrossWorks for ARM 5.4.2 (Windows),
/opt/rowley/crossworks_for_arm_5.4.2 (Linux).

Examples:
  $(basename "$0") Coremark_1060
  $(basename "$0") -P "O3,O2" -p 000123456789 Coremark_1160CM7 Coremark_1060
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
		CrossWorksDir="/opt/rowley/crossworks_for_arm_5.4.2"
		ExeSuffix=""
		;;
	*)	# Windows (Git Bash, MSYS, Cygwin)
		CrossWorksDir="C:/Program Files/Rowley CrossWorks for ARM 5.4.2"
		ExeSuffix=".exe"
		;;
esac

SolutionFile="Coremark_Extended.hzp"
TargetInterface="CMSIS-DAP"
ProbeSerial=""
BuildConfig="Release"
WaitTimeoutSec=600
SecondaryCoreWaitSec=20
ProfileFilter=""
SkipLoad=0
StopOnError=0
DryRun=0
ListOnly=0
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
		-s|--solution|-C|--crossworks-dir|-i|--interface|-p|--probe|-c|--config|-t|--timeout|-w|--secondary-wait|-P|--profiles)
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
		-w|--secondary-wait)
			[[ "$Value" =~ ^[0-9]+$ ]] || Fail "Invalid secondary wait time '$Value'."
			SecondaryCoreWaitSec="$Value"
			;;
		-P|--profiles)         ProfileFilter="$Value" ;;
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
		Known=0
		for Profile in "${Profiles[@]}"; do
			[ "${Profile%%|*}" = "$FilterItem" ] && Known=1
		done
		[ $Known -eq 1 ] || Fail "Unknown profile '$FilterItem'. Available: $ProfileLabels"
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
		if [ ! -f "$Tool" ]; then
			Fail "'$Tool' not found. Use --crossworks-dir to set the CrossWorks installation directory."
		fi
	done
fi
for File in "$SolutionFile" "$WaitScript"; do
	[ -f "$File" ] || Fail "'$File' not found."
done

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
# ProjectNames:    all projects of the solution
# DepParent/Child: pairs of a project and a project that is listed in its debug_dependent_projects
#                  (the secondary core of a dual core device). CrossLoad downloads such a project
#                  together with its parent, so it is not started on its own. Projects which are only
#                  listed in project_dependencies (build order) are still selectable on their own.
ProjectNames=()
DepParent=()
DepChild=()

while IFS='|' read -r Kind Name Dependencies; do
	case "$Kind" in
		P)	ProjectNames+=("$Name")
			;;
		D)	IFS=';' read -r -a Items <<< "$Dependencies"
			for Item in "${Items[@]}"; do
				Item="${Item//[[:space:]]/}"
				if [ -n "$Item" ]; then
					DepParent+=("$Name")
					DepChild+=("$Item")
				fi
			done
			;;
	esac
done < <(awk '
	{ gsub(/\r/, "") }
	/<project[ \t]+Name="/ {
		Line = $0
		sub(/^.*<project[ \t]+Name="/, "", Line)
		sub(/".*$/, "", Line)
		Project = Line
		print "P|" Project
		next
	}
	/<\/project>/ { Project = "" }
	Project != "" && /debug_dependent_projects="[^"]+"/ {
		Line = $0
		sub(/^.*debug_dependent_projects="/, "", Line)
		sub(/".*$/, "", Line)
		print "D|" Project "|" Line
	}
' "$SolutionFile")

[ ${#ProjectNames[@]} -gt 0 ] || Fail "No projects found in $SolutionFile."

IsSecondary ()
{
	local Child
	for Child in "${DepChild[@]}"; do
		[ "$Child" = "$1" ] && return 0
	done
	return 1
}

# Print the secondary core projects of project $1 (one per line)
SecondariesOf ()
{
	local Index
	for Index in "${!DepParent[@]}"; do
		[ "${DepParent[$Index]}" = "$1" ] && printf '%s\n' "${DepChild[$Index]}"
	done | awk '!Seen[$0]++'
}

BuildProjects=()
for ProjectName in "${ProjectNames[@]}"; do
	IsSecondary "$ProjectName" || BuildProjects+=("$ProjectName")
done

echo "Main projects:"
for ProjectName in "${BuildProjects[@]}"; do
	Secondaries=()
	mapfile -t Secondaries < <(SecondariesOf "$ProjectName")
	if [ ${#Secondaries[@]} -gt 0 ]; then
		echo "- $ProjectName (+ secondary core: ${Secondaries[*]})"
	else
		echo "- $ProjectName"
	fi
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
		Found=0
		for ProjectName in "${BuildProjects[@]}"; do
			[ "$ProjectName" = "$Argument" ] && Found=1
		done
		[ $Found -eq 1 ] || Fail "'$Argument' is not a main project of $SolutionFile."
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
LastState=""

PrintSummary ()
{
	local Index Failed=0
	echo ""
	echo "================================ Summary ================================"
	for Index in "${!ResultNames[@]}"; do
		printf '%-42s %-28s %5s s\n' "${ResultNames[$Index]}" "${ResultStates[$Index]}" "${ResultSeconds[$Index]}"
		case "${ResultStates[$Index]}" in
			FINISHED|BUILT|"DRY RUN") ;;
			*) Failed=1 ;;
		esac
	done
	echo "========================================================================="
	return $Failed
}

trap 'echo; echo "Interrupted."; PrintSummary; exit 130' INT

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

# LoadAndWait <project> <number of secondary cores>
# Sets LastState. Returns 0 if the benchmark has finished.
LoadAndWait ()
{
	local Project="$1"
	local SecondaryCount="$2"
	local CrossloadArgs=(-target "$TargetInterface")
	[ -n "$ProbeSerial" ] && CrossloadArgs+=(-setprop "Use Serial Number=$ProbeSerial")
	CrossloadArgs+=(-solution "$SolutionFile" -project "$Project" -config "$BuildConfig")

	# The script gets its parameters through a generated header.
	local SecondaryWaitMs=0
	[ "$SecondaryCount" -gt 0 ] && SecondaryWaitMs=$((SecondaryCoreWaitSec * 1000))
	mkdir -p "$(dirname "$RunScript")"
	rm -f "$ResultFile"
	{
		printf 'var WaitTimeoutMs = %d;\n'   $((WaitTimeoutSec * 1000))
		printf 'var SecondaryWaitMs = %d;\n' "$SecondaryWaitMs"
		printf 'var ResultFile = "%s";\n'    "$(ToMixedPath "$ResultFile")"
		cat "$WaitScript"
	} > "$RunScript"

	# Secondary cores are downloaded by CrossLoad itself (debug_dependent_projects).
	echo "Downloading $Project and waiting for the benchmark to finish"
	# stdin is closed, so that CrossLoad cannot get stuck at its interactive prompt if the script fails.
	if ! Run "$Crossload" "${CrossloadArgs[@]}" -debug -script "$(ToWindowsPath "$RunScript")" < /dev/null; then
		LastState="LOAD FAILED"
		return 1
	fi

	if [ "$DryRun" = 1 ]; then
		LastState="DRY RUN"
		return 0
	fi
	if [ -f "$ResultFile" ]; then
		LastState="$(tr -d '\r\n' < "$ResultFile")"
	else
		LastState="NO RESULT"
	fi
	[ "$LastState" = "FINISHED" ]
}

for Project in "${SelectedProjects[@]}"; do
	Secondaries=()
	mapfile -t Secondaries < <(SecondariesOf "$Project")

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

		if ! BuildProject "$Project" "$Level" "$Lto" "$Marker"; then
			LastState="BUILD FAILED"
		elif [ "$SkipLoad" = 1 ]; then
			LastState="BUILT"
		else
			LoadAndWait "$Project" "${#Secondaries[@]}"
		fi

		ResultNames+=("$RunName")
		ResultStates+=("$LastState")
		ResultSeconds+=("$((SECONDS - StartSeconds))")
		echo "$RunName: $LastState"

		case "$LastState" in
			FINISHED|BUILT|"DRY RUN") ;;
			*)	if [ "$StopOnError" = 1 ]; then
					PrintSummary
					exit 1
				fi
				;;
		esac
	done
done

PrintSummary
