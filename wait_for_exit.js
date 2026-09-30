// wait_for_exit.js
//
// Debug script for CrossLoad (crossload -debug -script <file>), used by StartTest.sh.
// It lets the benchmark run and waits until the application has finished.
// Afterwards the debug session is ended, so that StartTest.sh can continue with the next
// project / compiler profile.
//
// "Finished" means: main() has returned and the startup code (thumb_crt0.s) has reached
// its endless loop "exit_loop". CoreMark has printed all results and "Coremark done" by then.
// As a fallback the breakpoint is set on portable_fini() (called right before main() returns).
//
// Parameters
// ----------
// StartTest.sh prepends a small generated header which defines the following variables.
// If they are not defined (script started manually) the defaults below are used.
//   WaitTimeoutMs   - maximum time to wait for the application to finish
//   SecondaryWaitMs - additional time to wait after the primary core has finished. The
//                     secondary core of a dual core device (e.g. the Cortex-M4 of an iMXRT1160)
//                     runs independently and is not observed by this debug session.
//   ResultFile      - optional. The result (FINISHED, TIMEOUT or ERROR: ...) is written to this
//                     file, because CrossLoad cannot hand over an exit code from a script.
//
// Debug API: CrossWorks script class "Debug" (ide_script_class_Debug.htm)
//
// NOTE: The CrossWorks script engine understands only a subset of JavaScript (tested with
// crossscript.exe 5.4.2). Not supported are: try/catch/throw, "===" / "!==", "continue" and
// chained calls like new Date().getTime(). Keep this file within that subset.

var WaitTimeoutMs   = (typeof WaitTimeoutMs   == "undefined") ? 600000 : WaitTimeoutMs;
var SecondaryWaitMs = (typeof SecondaryWaitMs == "undefined") ? 0      : SecondaryWaitMs;
var ResultFile      = (typeof ResultFile      == "undefined") ? ""     : ResultFile;

function finish (text)
{
	Debug.echo ("RESULT: " + text);
	if (ResultFile != "")
		CWSys.writeStringToFile (ResultFile, text);
	Debug.quit ();
}

// Busy wait. Debug.wait() cannot be used for this, because it returns immediately while the
// target is halted.
function sleepMs (ms)
{
	var start = new Date ();
	var end   = start.getTime () + ms;
	var now   = new Date ();
	while (now.getTime () < end)
		now = new Date ();
}

// Hardware breakpoints are mandatory here. The code runs from TCM (.text_tcm), which is copied
// from flash by the startup code. A software breakpoint set before that copy would be overwritten.
// exit_loop lives in flash (.init), where software breakpoints are not possible at all.
// Returns the breakpoint number (> 0) or 0 if the breakpoint could not be set.
function setBreakpoint (symbol)
{
	var number = Debug.breakexpr (symbol, 0, true);
	if (number > 0)
	{
		Debug.echo ("Hardware breakpoint " + number + " set on " + symbol);
		return number;
	}
	Debug.echo ("Could not set breakpoint on " + symbol + " (result " + number + ")");
	return 0;
}

function main ()
{
	var symbol     = "exit_loop";
	var breakpoint = setBreakpoint (symbol);
	if (breakpoint <= 0)
	{
		symbol     = "portable_fini";
		breakpoint = setBreakpoint (symbol);
	}
	if (breakpoint <= 0)
	{
		finish ("ERROR: no breakpoint could be set");
		return;
	}

	// Depending on how the session was started the target is either halted (e.g. at the start
	// of the application) or already running.
	if (Debug.stopped ())
		Debug.go ();

	Debug.echo ("Waiting for CoreMark to finish (" + symbol + ", timeout " + (WaitTimeoutMs / 1000) + " s) ...");
	var hit = Debug.wait (WaitTimeoutMs);
	if (hit <= 0)
	{
		Debug.breaknow ();
		finish ("TIMEOUT");
		return;
	}
	Debug.echo ("CoreMark finished (stopped at " + symbol + ")");

	if (symbol == "portable_fini")
	{
		// portable_fini() prints "Coremark done" - give the core the chance to do so.
		Debug.deletebreak (0);
		Debug.go ();
		Debug.wait (1000);
	}

	if (SecondaryWaitMs > 0)
	{
		Debug.echo ("Waiting " + (SecondaryWaitMs / 1000) + " s for the secondary core ...");
		sleepMs (SecondaryWaitMs);
	}

	finish ("FINISHED");
}

main ();
