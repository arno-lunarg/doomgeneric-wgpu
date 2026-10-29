---
name: tutorial-step
description: Write the next WebGPU tutorial page (tutorial/stepN.html) for the Doom port, verify its reference solution builds and runs, publish it as an artifact. Use when the learner asks for the next step or a new tutorial page.
---

# Write tutorial step N

The learner writes the code; the page guides. Follow the format of `tutorial/step1.html` exactly (same CSS, TOC with progress checkboxes, section anatomy). Copy it as the starting point.

## Before writing

1. Read `AGENT.md` (curriculum, notes for later steps, project facts) and the learner's current code in `src/`. Build on their actual `Gpu` struct and function names, not on the step 1 reference.
2. List every API the step needs. Grep each in `third_party/dawn/include/dawn/webgpu.h`; note the `_INIT` defaults that matter.
3. Write a reference solution in a scratch copy of the learner's files (never in `src/`). Build it, run it with `tools/capture.ps1` on D3D12 and `-vulkan`, look at the PNG. Break it once on purpose to collect a real error message for the page.
4. Revert any temporary edits; `git status` must show `src/` untouched by you.

## Section anatomy

Each `§n` section has, in order:

- **Goal**: one sentence.
- **Refs**: Vulkan analog chip, LearnWebGPU chapter, spec or header-docs link.
- **API list**: names only.
- Prose: what differs from Vulkan and why. Short.
- **Trap** callout: mistakes a Vulkan programmer makes here.
- **Checkpoint** callout: what they should see.
- **Solution** in a closed `<details>`, copied from the verified reference.

Sections are sized for 15–40 minutes. End with a debugging section that exercises a new tool on this step's code.

## Code in the source skeleton

If the step needs new files or hooks in the learner's project, add only non-WebGPU plumbing plus `TODO(§n)` markers naming the section. The project must build and run with the TODOs unimplemented.

## Publish

- Publish `tutorial/stepN.html` with the Artifact tool (new artifact, icon `code`).
- Add the URL to the artifact list in `AGENT.md` and update the curriculum status.
- Add the page to `doomgeneric-wgpu.sln` Solution Items.
