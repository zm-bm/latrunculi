# Candidate revisions

Keep the selected change in one unpushed, engine-only commit based directly on the operational baseline. Record its branch and full candidate and baseline SHAs in the retained result summary; the development board may use short SHAs. Build and test that exact commit in a clean, isolated worktree. Keep the inputs and summary that produced the decision.

Preserve reviewed, tested, and published commits and their Git references. A fix, rebase, or adaptation requires a new commit on the current baseline and fresh evidence for anything it may affect. Keep the experiment ID and old revision in the record; acceptance of the old revision does not carry over. Never overwrite or rewrite a published or reviewed candidate to reuse its identity.
