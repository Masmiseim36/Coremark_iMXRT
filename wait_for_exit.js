// Wait until CoreMark returns from main() and CRT0 reaches exit_loop
// (thumb_crt0.s line 310). Then end the CrossLoad debug session so
// StartTest.sh can continue with the next compiler profile.
//
// Debug API: CrossWorks script class "Debug"

var WaitTimeoutMs = 600000;
var breakpoint = Debug.breakexpr ("portable_fini");

if (breakpoint <= 0)
{
	Debug.echo ("Could not set breakpoint on portable_fini: " + breakpoint);
	breakpoint = Debug.breakexpr ("exit_loop");
	if (breakpoint <= 0)
	{
		Debug.echo ("Could not set breakpoint on exit_loop: " + breakpoint);
		Debug.quit ();
	}
}

Debug.echo ("Waiting for CoreMark to finish (exit_loop)...");
//Debug.go();

var hit = Debug.wait (WaitTimeoutMs);
if (hit <= 0)
{
	Debug.echo("Timeout waiting for exit_loop");
	Debug.breaknow();
}
else
{
	Debug.echo("CoreMark finished (breakpoint " + hit + ")");
}

Debug.quit();
