#!/bin/sh

# Copyright 2015-2022 Rivoreo

# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.


# Translated from scripts/MakeHeader.py which was originally written by Hisham
# Muhammad:
# (C) 2004-2019 Hisham Muhammad
# Released under the GNU GPL.


if [ $# != 1 ]; then
	printf "Usage: %s <name>.c\\n" "$0" 1>&2
	exit 255
fi
if [ "${1%.c}" = "$1" ]; then
	echo "Input file must have suffix '.c'" 1>&2
	exit 255
fi
if [ ! -f "$1" ]; then
	printf "Input file '%s' doesn't exist or not a regular file\\n" "$1" 1>&2
	exit 1
fi
state=any
static=
name="${1##*/}"
name="${name%.c}"
selfheader="#include \"$name.h\""
echo "/* Do not edit this file. It was automatically generated. */"
printf '\n#ifndef HEADER_%s\n#define HEADER_%s\n' "$name" "$name"
is_blank=
IFS=
while read -r line; do case "$state" in
	any)
		case "$line" in
			"/*{")
				state=copy
				;;
			"$selfheader")
				;;
			*"#include"*)
				;;
			*"#error"*|*"#warning"*)
				;;
			*"htop - "*.c)
				printf %s\\n "${line%c}h"
				;;
			"struct "*" "*\;)
				is_blank=
				printf 'extern %s\n' "$line"
				state=skipone
				;;
			"struct "*|"typedef "*)
				[ "${line%\{}" = "$line" ] && is_blank=1 || state=skip
				;;
			*"static "*)
				if [ "${line%\{}" != "$line" ]; then
					state=skip
					static=1
				else
					is_blank=1
				fi
				;;
			*"extern "*\;*)
				state=skipone
				;;
			*"= {")
				static=
				is_blank=
				printf 'extern %s;\n' "${line%?= \{}"
				state=skip
				;;
			*"{")
				static=
				is_blank=
				#printf 'extern %s;\n' "${line%\)*\{})" | sed -E "s/ (extern|inline)//g"
				printf '%s;\n' "${line%\)*\{})" | sed -E "s/(extern|inline) //g"
				state=skip
				;;
			*" = "*)
				static=
				is_blank=
				printf 'extern %s;\n' "${line%% = *}"
				;;
			*[_a-zA-Z0-9]\;|*\[*\]\;)
				static=
				is_blank=
				printf 'extern %s\n' "$line"
				;;
			*[_a-zA-Z0-9]"; /*"*"*/")
				static=
				is_blank=
				printf 'extern %s;\n' "${line%%;*}"
				;;
			??*)
				static=
				is_blank=
				printf %s\\n "$line"
				;;
			"")
				[ -z "$is_blank" ] && echo
				is_blank=1
				;;
			*)
				printf %s\\n "$line"
				is_blank=
				;;
		esac
		;;
	copy)
		is_blank=
		case "$line" in
			"}*/")
				state=any
				;;
			*)
				printf %s\\n "$line"
				;;
		esac
		;;
	skip)
		is_blank=
		case "$line" in
			"} "*" = {")
				;;
			"}"*)
				#[ -n "$static" ] && state=skipone || state=any
				[ -n "$static" ] && is_blank=1
				state=any
				static=
				;;
		esac
		;;
	skipone)
		is_blank=
		state=any
		;;
esac done < "$1"
printf '\n#endif\n'
