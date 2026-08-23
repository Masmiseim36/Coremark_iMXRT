// wait_for_exit.js
//
// Debug script for CrossLoad, used by StartTest.sh:
//
//     crossload ... -debug -break main -script wait_for_exit.js
//
// It lets the benchmark run on all cores of the device and waits until it has finished on every
// core. Afterwards the debug session is ended, so that StartTest.sh can continue with the next
// project / compiler profile. A dual core device (iMXRT1160 / iMXRT1170) is debugged with a single
// session: CrossLoad downloads the project given with -project and its "Debug Additional
// Projects" (the other core), and the script switches between the cores.
//
// Sequence
// --------
// 1. CrossLoad runs every core up to main() (option "-break main"). That happens asynchronously
//    to this script, so the script first waits until the core has stopped.
// 2. A hardware breakpoint is set on "exit_loop", the endless loop of the startup code
//    (thumb_crt0.s) which is reached when main() has returned. CoreMark has printed all results
//    and "Coremark done" by then.
// 3. All cores are started and the script waits for the breakpoint of each core.
//
// Parameters
// ----------
// StartTest.sh prepends a small generated header which defines the following variables.
// If they are not defined (script started manually) the defaults below are used.
//   WaitTimeoutMs   - safety limit for the time to wait for the application to finish. The
//                     end of the benchmark is detected by the breakpoint, the timeout only
//                     prevents that a hung target blocks the whole test.
//   ResultFile      - optional. The result is written to this file, because CrossLoad cannot hand
//                     over an exit code from a script. One line per core: "<debug project>|<state>"
//                     with the state FINISHED, TIMEOUT or ERROR: ...
//
// Notes on the CrossWorks script engine (CrossWorks 5.5 nightly build with multi-core support)
// --------------------------------------------------------------------------------------------
// - Only a subset of JavaScript is understood: no try/catch/throw, no "===" / "!==", no "continue"
//   and no chained calls like new Date().getTime().
// - Debug.breakexpr (expression, temporary, count, hardware) returns the breakpoint number. If the
//   symbol is unknown it prints an error and returns the expression (a string), not 0.
//   Debug.wait (ms) returns the number of the breakpoint that was hit, 0 on a timeout or if the
//   core was stopped by Debug.breaknow().
// - Debug.go() / Debug.evaluate() / Debug.where() fail with "not stopped" while the core runs.
// - After Debug.setdebugproject() the selected core may still be running up to main(): wait with
//   Debug.wait() until Debug.stopped() is true before using it.
// - Before the first Debug.setdebugproject() the current debug project is the last one of the
//   list, not the one given with -project. The script therefore always selects a core explicitly.
//
// Debug API: CrossWorks script class "Debug" (ide_script_class_Debug.htm)

var WaitTimeoutMs = (typeof WaitTimeoutMs == "undefined") ? 600000 : WaitTimeoutMs;
var ResultFile    = (typeof ResultFile    == "undefined") ? ""     : ResultFile;

// Maximum time for a core to run up to main() after the download
var StartupWaitMs = 15000;

var Cores       = [];   // debug projects, one per core
var States      = [];   // result of each core
var Breakpoints = [];   // number of the breakpoint at the end of the benchmark of each core

function now ()
{
	var d = new Date ();
	return d.getTime ();
}

// Make core i the current one
function select (i)
{
	if (Cores.length > 1)
		Debug.setdebugproject (Cores[i]);
}

function finish ()
{
	var text = "";
	var i;
	for (i = 0; i < Cores.length; i++)
	{
		Debug.echo ("RESULT: " + Cores[i] + " " + States[i]);
		text += Cores[i] + "|" + States[i] + "\n";
	}
	if (ResultFile != "")
		CWSys.writeStringToFile (ResultFile, text);
	Debug.quit ();
}

function main ()
{
	var i;
	var failed = false;

	Cores = Debug.getdebugprojects ();
	for (i = 0; i < Cores.length; i++)
		States[i] = "ERROR: not started because of another core";

	// 1. Wait until every core has stopped at main(), then set the breakpoint at the end of the
	//    benchmark. It is a hardware breakpoint, because exit_loop lives in the XiP flash.
	for (i = 0; i < Cores.length; i++)
	{
		select (i);
		Debug.wait (StartupWaitMs);
		if (!Debug.stopped ())
		{
			Debug.breaknow ();
			States[i] = "ERROR: the core did not stop at main() (is CrossLoad started with -break main?)";
			failed = true;
		}
		else
		{
			Breakpoints[i] = Debug.breakexpr ("exit_loop", false, 0, true);
			if (!(Breakpoints[i] > 0))
			{
				States[i] = "ERROR: the breakpoint at exit_loop could not be set";
				failed = true;
			}
		}
	}
	if (failed)
	{
		finish ();
		return;
	}

	// 2. Start all cores, then wait for each of them
	Debug.echo ("Waiting for CoreMark to finish (timeout " + (WaitTimeoutMs / 1000) + " s) ...");
	var deadline = now () + WaitTimeoutMs;
	for (i = 0; i < Cores.length; i++)
	{
		select (i);
		Debug.go ();
	}
	for (i = 0; i < Cores.length; i++)
	{
		select (i);
		var hit = Debug.wait (Math.max (deadline - now (), 1000));
		if (!Debug.stopped ())
		{
			Debug.breaknow ();
			Debug.wait (2000);
			States[i] = "TIMEOUT";
		}
		else if (hit != Breakpoints[i])
			States[i] = "ERROR: the core stopped for another reason than the end of the benchmark";
		else
			States[i] = "FINISHED";
	}
	finish ();
}

main ();
