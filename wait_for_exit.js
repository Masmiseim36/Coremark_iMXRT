// wait_for_exit.js
//
// Debug script for CrossLoad, used by StartTest.sh:
//
//     crossload ... -debug -break main -script wait_for_exit.js
//
// It lets the benchmark run on one or more cores and waits until the application has finished on
// every core. Afterwards the debug session is ended, so that StartTest.sh can continue with the
// next project / compiler profile.
//
// Sequence
// --------
// 1. CrossLoad downloads the application(s) and runs each core up to main() (option "-break main").
//    That happens asynchronously to this script, so the script first waits until the core has stopped.
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
//   DebugProjects   - the debug projects ("<project>_<configuration>") of all cores, separated by
//                     ";". Only if more than one is given, the script switches between them with
//                     Debug.setdebugproject(). This needs a CrossWorks version which supports
//                     multi-core debugging in CrossLoad (the Debug.setdebugproject() method).
//
// Notes on the CrossWorks script engine (verified with CrossWorks 5.4.2 and 5.5 nightly builds)
// ---------------------------------------------------------------------------------------------
// - Only a subset of JavaScript is understood: no try/catch/throw, no "===" / "!==", no "continue"
//   and no chained calls like new Date().getTime().
// - Debug.breakexpr() has the signature (expression, hardware, count). This is NOT the order
//   given in the CrossWorks documentation. With (expression, 0, true) the breakpoint is a software
//   breakpoint with an ignore count of 1 and never stops the application. The function does not
//   return a breakpoint number, so the result cannot be checked.
// - The return value of Debug.wait() is not reliable (0 on a timeout, but also when the break was
//   caused by a breakpoint without a number). Debug.stopped() is used instead.
// - Debug.go() / Debug.evaluate() / Debug.where() fail with "not stopped" while the target runs.
// - After Debug.setdebugproject() the newly selected core may still be running up to main():
//   wait with Debug.wait() until Debug.stopped() is true before using it.
//
// Debug API: CrossWorks script class "Debug" (ide_script_class_Debug.htm)

var WaitTimeoutMs   = (typeof WaitTimeoutMs   == "undefined") ? 600000 : WaitTimeoutMs;
var ResultFile      = (typeof ResultFile      == "undefined") ? ""     : ResultFile;
var DebugProjects   = (typeof DebugProjects   == "undefined") ? ""     : DebugProjects;

// Maximum time for a core to run up to main() after the download
var StartupWaitMs = 15000;

var Cores   = [];     // debug project names, "" for a single core without switching
var States  = [];     // result of each core
var Switch  = false;  // true if more than one core is controlled

function now ()
{
	var d = new Date ();
	return d.getTime ();
}

// Make core i the current one
function select (i)
{
	if (Switch)
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

	if (DebugProjects != "")
		Cores = DebugProjects.split (";");
	else
		Cores = [""];
	Switch = (Cores.length > 1);

	// 1. Wait until every core has stopped at main()
	for (i = 0; i < Cores.length; i++)
	{
		States[i] = "";
		select (i);
		Debug.wait (StartupWaitMs);
		if (!Debug.stopped ())
		{
			Debug.breaknow ();
			States[i] = "ERROR: the core did not stop at main() (is CrossLoad started with -break main?)";
			failed = true;
		}
	}
	if (failed)
	{
		for (i = 0; i < Cores.length; i++)
			if (States[i] == "")
				States[i] = "ERROR: not started because of another core";
		finish ();
		return;
	}

	// 2. Hardware breakpoint, because exit_loop lives in the XiP flash
	for (i = 0; i < Cores.length; i++)
	{
		select (i);
		Debug.breakexpr ("exit_loop", true, 0);
	}

	// 3. Start all cores, then wait for each of them
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
		var remaining = deadline - now ();
		if (remaining < 1000)
			remaining = 1000;
		Debug.wait (remaining);
		if (Debug.stopped ())
			States[i] = "FINISHED";
		else
		{
			Debug.breaknow ();
			Debug.wait (2000);
			States[i] = "TIMEOUT";
		}
	}
	finish ();
}

main ();
