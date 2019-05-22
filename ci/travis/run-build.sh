#!/bin/bash
set -e

. ./ci/travis/lib.sh

build_default() {
	make ${DEFCONFIG_NAME}
	make -j`getconf _NPROCESSORS_ONLN` $IMAGE UIMAGE_LOADADDR=0x8000
}

build_compile_test() {
	export COMPILE_TEST=y
	make ${DEFCONFIG_NAME}
	make -j`getconf _NPROCESSORS_ONLN`
}

build_checkpatch() {
	if [ -n "$TRAVIS_BRANCH" ]; then
		git fetch origin +refs/heads/${TRAVIS_BRANCH}:${TRAVIS_BRANCH}
	fi
	COMMIT_RANGE=$([ "$TRAVIS_PULL_REQUEST" == "false" ] &&  echo HEAD || echo ${TRAVIS_BRANCH}..)
	scripts/checkpatch.pl --git ${COMMIT_RANGE} \
		--ignore FILE_PATH_CHANGES \
		--ignore LONG_LINE \
		--ignore LONG_LINE_STRING \
		--ignore LONG_LINE_COMMENT
}

build_dtb_build_test() {
	for file in $DTS_FILES; do
		make ${DTS_PREFIX}`basename $file | sed  -e 's\dts\dtb\g'` || exit 1
	done
}

branch_contains_commit() {
	local commit="$1"
	local branch="$2"
	git merge-base --is-ancestor $commit $branch &> /dev/null
}

commit_is_merge() {
	local cm="$1"
	cm="$(git cat-file -p "$cm" 2>/dev/null | grep parent | wc -l)"
	[ "$cm" != "1" ]
}

__update_git_ref() {
	local ref="$1"
	git fetch origin +refs/heads/${ref}:${ref}
}

__handle_sync_with_master() {
	local dst_branch="$1"
	local method="$2"
	for ref in master ${dst_branch} ; do
		__update_git_ref "$ref" || {
			echo_red "Could not fetch branch '$dst_branch'"
			return 1
		}
	done

	if [ "$method" == "fast-forward" ] ; then
		git checkout ${dst_branch}
		git merge --ff-only master || {
			echo_red "Failed while syncing master over '$dst_branch'"
			return 1
		}
		return 0
	fi

	if [ "$method" == "cherry-pick" ] ; then
		# FIXME: kind of dumb, the code below; maybe do this a bit neater
		local cm="$(git log "${dst_branch}~1..${dst_branch}" | grep "cherry picked from commit" | awk '{print $5}' | cut -d')' -f1)"
		[ -n "$cm" ] || {
			echo_red "Top commit in branch '${dst_branch}' is not cherry-picked"
			return 1
		}
		branch_contains_commit "$cm" "master" || {
			echo_red "Commit '$cm' is not in branch master"
			return 1
		}

		git checkout ${dst_branch}
		for cm in $(git rev-list "${cm}..master") ; do
			! commit_is_merge "${cm}" || continue
			git cherry-pick -x "${cm}" || {
				echo_red "Failed to cherry-pick commit $cm"
				return 1
			}
		done

		return 0
	fi
}

build_sync_branches_with_master() {
	# make sure this is on master, and not a PR
	[ -n "$TRAVIS_PULL_REQUEST" ] || return 0
	[ "$TRAVIS_PULL_REQUEST" == "false" ] || return 0
	[ "$TRAVIS_BRANCH" == "master" ] || return 0

	# support sync-ing up to 100 branches; should be enough
	for iter in $(seq 1 100) ; do
		local branch="$(eval "echo \$BRANCH${iter}")"
		[ -n "$branch" ] || break
		local dst_branch="$(echo $branch | cut -d: -f1)"
		[ -n "$dst_branch" ] || break
		local method="$(echo $branch | cut -d: -f2)"
		[ -n "$method" ] || break
		__handle_sync_with_master "$dst_branch" "$method"
	done
}

BUILD_TYPE=${BUILD_TYPE:-default}

build_${BUILD_TYPE}
