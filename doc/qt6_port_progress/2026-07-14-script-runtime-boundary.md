# Qt 6 Script Runtime Boundary

The Qt 6 Script Console now dispatches evaluation through ScriptEngine::Executor
so the console event loop can receive Ctrl+Y while JavaScript is running.
ScriptEngine::interrupt() calls QJSEngine::setInterrupted(true), and the
executor reports completion back to the GUI thread.

The Qt 6 view(image) and view(level) bridge is explicitly marshalled back to
the panel's owning GUI thread with a blocking queued invocation. This prevents
FlipBook creation and viewer mutation from occurring on the evaluator thread.

This is an incremental boundary, not a release signoff: QJSEngine construction
and the legacy QObject binding surface still require a future persistent
worker-owned engine refactor. That refactor must preserve one-engine/one-thread
ownership for construction, evaluation, interruption, and destruction, and must
move every main-thread application operation across a queued bridge.

Required manual acceptance remains:

- run an infinite-loop command in the Qt 6 console;
- confirm the window repaints and Ctrl+Y is accepted within 100 ms;
- confirm the interruption and a subsequent command complete within 2 seconds;
- confirm view(image) and view(level) execute on the GUI thread;
- record supported and unsupported bindings separately from the broad script
  fixture aggregate.

The local build validates compilation and guardrails. A headless macOS session
cannot provide the interactive keyboard or FlipBook evidence.
