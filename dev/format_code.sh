#!/usr/bin/env bash
set -eo pipefail

# Formats files according our coding style with clang-format.
#
# Usage:
#   - Make sure the executables "clang-format" and "git" are available in PATH.
#   - Run the command "./dev/format_code.sh" in the root of the repository.
#   - To run clang-format in a docker-container, use the "--docker" parameter.
#   - To format all files (instead of only modified ones), add the "--all"
#     parameter. This is intended only for LibrePCB maintainers, don't use it!

for i in "$@"
do
case $i in
    --docker)
    DOCKER="--docker"
    shift
    ;;
    --all)
    ALL="--all"
    shift
    ;;
esac
done

echo "Formatting files with clang-format..."

if [ "$DOCKER" == "--docker" ]; then
  DOCKER_IMAGE=librepcb/clang-format:6
  REPO_ROOT=$(git rev-parse --show-toplevel)

  if [ "$(docker images -q $DOCKER_IMAGE | wc -l)" == "0" ]; then
    echo "Building clang-format container..."
    cd "$REPO_ROOT/dev/clang-format"
    docker build . -t librepcb/clang-format:6
    cd -
  fi

  echo "[Re-running format_code.sh inside Docker container]"
  docker run --rm -t -i --user $(id -u):$(id -g) \
    -v "$REPO_ROOT:/code" \
    $DOCKER_IMAGE \
    /bin/bash -c "cd /code && dev/format_code.sh $ALL"

  echo "[Docker done.]"
  exit 0
fi

COUNTER=0

for dir in apps/ libs/librepcb/ tests/unittests/
do
  if [ "$ALL" == "--all" ]; then
    TRACKED=`git ls-files -- "${dir}**.cpp" "${dir}**.hpp" "${dir}**.h"`
  else
    # Only files which differ from the master branch
    TRACKED=`git diff --name-only master -- "${dir}**.cpp" "${dir}**.hpp" "${dir}**.h"`
  fi
  UNTRACKED=`git ls-files --others --exclude-standard -- "${dir}**.cpp" "${dir}**.hpp" "${dir}**.h"`
  for file in $TRACKED $UNTRACKED
  do
    # Note: Do NOT use in-place edition of clang-format because this causes
    # "make" to detect the files as changed every time, even if the content was
    # not modified! So we only overwrite the files if their content has changed.
    OLD_CONTENT=`cat "$file"`
    NEW_CONTENT=`clang-format -style=file "$file"`
    if [ "$NEW_CONTENT" != "$OLD_CONTENT" ]
    then
        printf "%s\n" "$NEW_CONTENT" > "$file"
        echo "[M] $file"
        COUNTER=$((COUNTER+1))
    else
        echo "[ ] $file"
    fi
  done
done

echo "Finished: $COUNTER files modified."
