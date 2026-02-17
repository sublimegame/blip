# Open or update PR

I want to make sure there's a PR that's in a good state for the local branch.

If there are local changes (not yet committed), ask if you should commit those changes, discard them, or do things differently. You'll need an answer before continuing.

Now if there are no local changes:

Git fetch and make sure the branch is rebased against origin/main. Rebase the branch against origin/main if not, fixing conflicts if needed. Ask if you're unsure about how some conflicts should be solved.

Now if the branch is correctly rebased against origin/main:

Please use Git CLI to check if there's already a PR for this branch and create it otherwise.

Use ⁠ git diff origin/main..HEAD ⁠ to get the complete diff from origin/main.

Use the diff to set a great title that describes the content of the PR, the main problem it's addressing.
If there's already a title or description, make sure it covers diff changes, don't hesitate to update them if needed.

The description should be no longer than 50 lines, the shorter the better (it's ok if it's one line).

Check issues using GitHub CLI to see if some ⁠ Fixes #ISSUE-NUMBER ⁠ or ⁠ Contributes to #ISSUE-NUMBER ⁠ could be added. Always put those statements at the very top of the description.

Do not include TODO lists, unless explicitly requested. Do not erase existing TODO lists.

Do not run any CI tests locally. The PR might not be fully ready for review, and that's ok.

Always post the PR URL and branch name at the end of your work.