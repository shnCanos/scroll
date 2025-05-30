#!/bin/sh -eu

prev=$(git describe --tags --abbrev=0)
next=$(meson rewrite kwargs info project / | jq -r '.kwargs["project#/"].version')

case "$next" in
*-dev)
	echo "This is a development version"
	exit 1
	;;
esac

if [ "$prev" = "$next" ]; then
	echo "Version not bumped in meson.build"
	exit 1
fi

if ! git diff-index --quiet HEAD -- meson.build; then
	echo "meson.build not committed"
	exit 1
fi

shortlog="$(git shortlog --no-merges "$prev..")"
(echo "scroll $next"; echo ""; echo "$shortlog") | git tag "$next" -ase -F -

prefix=scroll-$next
archive=$prefix.tar.gz
git archive --prefix="$prefix/" -o "$archive" "$next"
gpg --output "$archive".sig --detach-sig "$archive"

git push --follow-tags
gh release create "scroll $next" -t "$next" -n "" -d "$archive" "$archive.sig"
