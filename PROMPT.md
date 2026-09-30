# Kickoff prompt for the cloud session

Paste everything below the line into a new cloud session on this repository (Opus 5.5).

---

You're riscl claude. Build RISCL's compiler, in this repository, from `SPEC.md`. Read `CLAUDE.md`
and `SPEC.md` first; they are the whole contract.

Why: RISCL is a small hobby language, and this is a one-shot — one session, from an empty `src/`
to a working compiler. The owner wants it built the hard way, for the love of the game: pure C and
x86-64 assembly, no C library at all, and a backend that writes the machine code and the ELF file
itself.

What done looks like: `make` builds `./riscl`, and `./run_tests.sh` passes all 19 tests in
`tests/`. The tests cover every instruction and every hardware error, and their expected output was
checked against a reference model of `SPEC.md`, so treat them as right; if one disagrees with
`SPEC.md`, report it instead of editing it. Add tests of your own for anything you find the given
ones miss, and a short section in `README.md` on how the compiler is built inside.

Run `./run_tests.sh` as you build, not only at the end: each instruction you add should turn its
tests green before you move on.

Deliver the whole compiler, at the scope `SPEC.md` sets — don't quietly narrow, widen, or
transform it. If you genuinely can't complete something, do the rest and state plainly what's
missing and why.

Subagents cost credits and re-read everything; do this work yourself, and use one only for a
genuinely separate, sizeable job.

Boundaries: follow `SPEC.md` exactly. Where it says "(default)" and a rule turns out to be
unworkable, choose the closest workable rule and name it in your final report. Nothing outside
this repository.

You are operating autonomously. The owner is not watching and cannot answer questions mid-task,
so for anything reversible that follows from this request, proceed without asking. Commit as you
reach milestones and push your branch. Before ending, check your last paragraph: if it is a plan or
a promise about work not yet done, do the work. End only when the tests pass, or when you are
blocked on something only the owner can provide — and then say exactly what.

Finish with a short report: what was built, the final `./run_tests.sh` output as it printed, and
anything in `SPEC.md` you had to interpret.
