#!/bin/bash -e
OUTER_PWD=$PWD
cd "$(dirname "$0")/.."

PKG_VERSION=$(node -e 'process.stdout.write(require("./package.json").version)')

BUMP_VERSION=
if [ "$1" == "" ]; then true
elif [ "$1" == "-major" ]; then BUMP_VERSION=major
elif [ "$1" == "-minor" ]; then BUMP_VERSION=minor
elif [ "$1" == "-patch" ]; then BUMP_VERSION=patch
else
  cat << _MESSAGE_ >&2
$0: Unexpected option $1
Usage: $0 [-major | -minor | -patch]
  -major    Bump major version. e.g. 1.2.3 => 2.0.0
  -minor    Bump minor version. e.g. 1.2.3 => 1.3.0
  -patch    Bump patch version. e.g. 1.2.3 => 1.2.4
  (nothing) Leave version in package.json unchanged ($PKG_VERSION)
_MESSAGE_
  echo -n "Current version on NPM: ••• "
  NPM_VERSION=$(npm show markdown-wasm version)
  echo -en "\rCurrent version on NPM: ${NPM_VERSION}"
  echo ""
  exit 1
fi

# checkout products so that npm version doesn't fail. These are regenerated anyways.
git checkout -- dist

# Make sure there are no uncommitted changes
if [ -n "$(git status --untracked-files=no --ignore-submodules=dirty --porcelain)" ]; then
  echo "There are uncommitted changes:" >&2
  git status -s --untracked-files=no --ignore-submodules=dirty
  exit 1
fi

# Bump version in package.js. This will fail and stop the script if git is not clean
if [ "$BUMP_VERSION" != "" ]; then
  npm --no-git-tag-version version "$BUMP_VERSION"
  PKG_VERSION=$(node -e 'process.stdout.write(require("./package.json").version)')
fi

# build
echo "" ; echo "wasmc -clean"
./node_modules/.bin/wasmc -clean

# test
echo "" ; echo "./test/test.sh"
./test/test.sh

# web site
cp dist/markdown.js dist/markdown.js.map dist/markdown.wasm ./docs/

# commit, tag and push git
echo "Ready to commit, publish & push:"
echo ""
if [[ "$PWD" != "$OUTER_PWD" ]]; then
  echo "  cd '$PWD'"
fi
cat << _MESSAGE_
  git commit -m "release v${PKG_VERSION}" .
  git tag "v${PKG_VERSION}"
  npm publish
  git push origin master "v${PKG_VERSION}"

_MESSAGE_
