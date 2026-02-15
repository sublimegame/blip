# Versioning

## TODO

- when store a world's code:
	- for a new world (no code in DB nor Git repo): do an initial commit with the default script and then another commit with the user's code.
	- for an existing world (code in DB but no Git repo): do an initial commit with the code present in DB and then another commit with the user's code.
	- for an existing world (code in Git repo): add a new commit with user's code.
- retrieve latest code (commit)
- publish code (create repo/commit)

- list last n commits
- reset to a given commit and return it

## IDEA

### Publish

Create a git tag per published version

## Tests

```sh
mkdir repo
cd repo
git init
```

```sh
mkdir code
cd code

git status
```
