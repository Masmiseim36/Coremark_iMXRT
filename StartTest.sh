#!/bin/bash

set -e

Path_Crossbuild="C:\Program Files\Rowley CrossWorks for ARM 5.4.2\bin\crossbuild.exe"
Path_Crossload="C:\Program Files\Rowley CrossWorks for ARM 5.4.2\bin\crossload.exe"
SolutionFile="Coremark_Extended.hzp"
WaitScript="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/wait_for_exit.js"


# Windows or linux
unameOut="$(uname -s)"
case "${unameOut}" in
	Linux*)
		Path_Crossbuild="/opt/rowley/crossworks_for_arm_5.4.2/bin/crossbuild"
		Path_Crossload="/opt/rowley/crossworks_for_arm_5.4.2/bin/crossload"
		;;
	Darwin*)
		;;
	CYGWIN*)
		;; 
	MINGW*)
		;; 
	MSYS_NT*)
		;;
	*)
		:
esac

if command -v cygpath >/dev/null 2>&1; then
	WaitScript="$(cygpath -w "$WaitScript")"
fi



declare -a CompilerProfiles=(
#	"NDEBUG;GCC_OPTIONS=\"### O3 LTO ###\"|Level 3|link_time_optimization=Yes"
#	"NDEBUG;GCC_OPTIONS=\"###_O3_###\"|Level 3|"
	"NDEBUG;GCC_OPTIONS=\"###_O2_###\"|Level 2|"
	"NDEBUG;GCC_OPTIONS=\"###_O1_###\"|Level 1|"
	"NDEBUG;GCC_OPTIONS=\"###_O0_###\"|Level 0|"
	"NDEBUG;GCC_OPTIONS=\"###_Debug_###\"|Debug|"
	"NDEBUG;GCC_OPTIONS=\"###_Optimize_For_Size_###\"|Optimize For Size|"
)

# Lies alle Projekt-Namen aus der Solution-Datei aus und speichere sie in einem Array.
mapfile -t ProjectNames < <(grep -oE '<project[[:space:]]+Name="[^"]+"' "$SolutionFile" | sed -E 's/.*Name="([^"]+)".*/\1/')

if [ ${#ProjectNames[@]} -eq 0 ]; then
	echo "Keine Projekte in $SolutionFile gefunden."
	exit 1
fi

# Erkenne Projekt-Abhängigkeiten aus den XML-Attributen debug_dependent_projects
# und project_dependencies und entferne die verknüpften Unterprojekte aus der Liste.
declare -a LinkedProjectNames=()
declare -a DependencyPairs=()
declare -a BuildProjects=()
declare -A SeenLinkedProjects=()
declare -A SeenDependencyPairs=()

while IFS= read -r DependencyEntry; do
	[ -z "$DependencyEntry" ] && continue

	ParentProject="${DependencyEntry%%|*}"
	DependencyList="${DependencyEntry#*|}"

	IFS=';' read -r -a DependencyItems <<< "$DependencyList"
	for DependencyItem in "${DependencyItems[@]}"; do
		DependencyItem="$(echo "$DependencyItem" | tr -d '[:space:]')"
		[ -z "$DependencyItem" ] && continue

		if [ -z "${SeenLinkedProjects[$DependencyItem]+x}" ]; then
			LinkedProjectNames+=("$DependencyItem")
			SeenLinkedProjects[$DependencyItem]=1
		fi

		DependencyPair="$ParentProject -> $DependencyItem"
		if [ -z "${SeenDependencyPairs[$DependencyPair]+x}" ]; then
			DependencyPairs+=("$DependencyPair")
			SeenDependencyPairs[$DependencyPair]=1
		fi
	done
done < <(awk '
		/<project[[:space:]]+Name="[^"]+"/ {
			if (project_name != "" && dependency_found) {
				print project_name "|" dependencies
			}

			project_name = ""
			dependency_found = 0
			dependencies = ""

			if (match($0, /<project[[:space:]]+Name="([^"]+)"/, m)) {
				project_name = m[1]
			}
			next
		}

		project_name != "" {
			if (match($0, /debug_dependent_projects="([^"]*)"/, m) && m[1] != "") {
				if (dependencies != "") {
					dependencies = dependencies ";"
				}
				dependencies = dependencies m[1]
				dependency_found = 1
			}

			if (match($0, /project_dependencies="([^"]*)"/, m) && m[1] != "") {
				if (dependencies != "") {
					dependencies = dependencies ";"
				}
				dependencies = dependencies m[1]
				dependency_found = 1
			}
		}

		END {
			if (project_name != "" && dependency_found) {
				print project_name "|" dependencies
			}
		}
	' "$SolutionFile")

for ProjectName in "${ProjectNames[@]}"; do
	IsLinkedProject=0
	for LinkedProjectName in "${LinkedProjectNames[@]}"; do
		if [ "$ProjectName" = "$LinkedProjectName" ]; then
			IsLinkedProject=1
			break
		fi
	done

	if [ $IsLinkedProject -eq 0 ]; then
		BuildProjects+=("$ProjectName")
	fi
done

echo "Hauptprojekte:"
for ProjectName in "${BuildProjects[@]}"; do
	echo "- $ProjectName"
done

echo "Projekt-Verknüpfungen:"
for DependencyPair in "${DependencyPairs[@]}"; do
	echo "- $DependencyPair"
done

# Auswahl des zu bauenden Projekts.
SelectionMode=""
if command -v zenity >/dev/null 2>&1; then
	SelectionMode="gui"
elif command -v kdialog >/dev/null 2>&1; then
	SelectionMode="gui"
fi

SelectedProjects=()

if [ "$SelectionMode" = "gui" ]; then
	if command -v zenity >/dev/null 2>&1; then
		mapfile -t SelectedProjects < <(zenity --list --title="Projekt auswählen" --text="Bitte ein oder mehrere Projekte auswählen:" --column="Projekt" "${BuildProjects[@]}" 2>/dev/null)
	elif command -v kdialog >/dev/null 2>&1; then
		mapfile -t SelectedProjects < <(kdialog --title "Projekt auswählen" --menu "Bitte ein Projekt auswählen:" 1 "${BuildProjects[@]}" 2>/dev/null)
	fi
fi

if [ ${#SelectedProjects[@]} -eq 0 ]; then
	echo ""
echo "Bitte wählen Sie ein Projekt aus:"
	echo "0) Alle Hauptprojekte bauen"
	for i in "${!BuildProjects[@]}"; do
		echo "$((i + 1))) ${BuildProjects[$i]}"
	done
	echo "Hinweis: Für den Testlauf kann z. B. Option 7 für Coremark_1060 verwendet werden."

	read -p "Auswahl: " UserSelection

	case "$UserSelection" in
		0)
			SelectedProjects=("${BuildProjects[@]}")
			;;
		*)
			if [ "$UserSelection" -ge 1 ] && [ "$UserSelection" -le ${#BuildProjects[@]} ]; then
				SelectedProjects=("${BuildProjects[$((UserSelection - 1))]}")
			else
				echo "Ungültige Auswahl."
				exit 1
			fi
			;;
	esac
fi

# Baue die ausgewählten Projekte und lade sie anschließend über CrossLoad auf das Ziel.
for ProjectName in "${SelectedProjects[@]}"; do
	for CompilerProfile in "${CompilerProfiles[@]}"; do
		IFS='|' read -r MacroValue OptimizationLevel LinkTimeOptimization <<< "$CompilerProfile"
		BuildArgs=(
			-verbose
			-config
			Release
			-project
			"$ProjectName"
			-sproperty
			c_preprocessor_definitions=$MacroValue
			-property
			"gcc_optimization_level=$OptimizationLevel"
			-rebuild
			-echo
		)

		if [ -n "$LinkTimeOptimization" ]; then
			BuildArgs+=( -sproperty link_time_optimization=Yes )
		fi

		BuildArgs+=( "$SolutionFile" )

		echo "Build Projekt: $ProjectName with compiler-options: ${BuildArgs[*]}"
		echo "$Path_Crossbuild ${BuildArgs[*]}"
		"$Path_Crossbuild" "${BuildArgs[@]}"

		echo "Lade Projekt auf das Ziel und warte auf Benchmark-Ende: $ProjectName"
		"$Path_Crossload" -target "CMSIS-DAP" -solution "$SolutionFile" -project "$ProjectName" -config Release -debug -script "$WaitScript"
	done
done