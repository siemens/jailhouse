# bash completion for jailhouse
#
# Copyright (c) Benjamin Block, 2014
# Copyright (c) Siemens AG, 2015
#
# Authors:
#  Benjamin Block <bebl@mageta.org>
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

# usage: - include the directory containing the `jailhouse`-tool into your
#          ${PATH}-variable
#        - source this tile `. tools/jailhouse_bashcompletion`
#        - alternatively you can but this file into your distributions
#          bash-completion directory
#
#          there is a broad variety of places where distris may put this:
#               - /usr/share/bash-completion/
#               - /etc/bash_completion.d/
#               - $BASH_COMPLETION_COMPAT_DIR
#               - $BASH_COMPLETION_DIR
#               - ...

# dependencies: - bash-completion (>= 1.3) package installed and activated in
#                 your bash

# bash-completion websites:
#     - http://bash-completion.alioth.debian.org/
#     - https://wiki.debian.org/Teams/BashCompletion
#     - http://anonscm.debian.org/cgit/bash-completion/bash-completion.git/

# only if jailhouse is in ${PATH}
( which jailhouse &>/dev/null )	&& \
{

# test for the required helper-functions
if ! type _filedir &>/dev/null; then
	# functions not defined
	#
	# The distributions seem to handle the inclusion of these function in a
	# broad variety of ways. To keep this script as simple as possible we
	# depend on this to work.

	return 1
fi

function _jailhouse_get_id() {
	local names ids cur prev quoted quoted_cur toks

	cur="${1}"
	prev="${2}"

	ids=""
	names=""

	_quote_readline_by_ref "$cur" quoted_cur

	# handle the '{ ID | [--name] NAME }'  part of cell-calls
	#
	# if we are at position 3 of the commnadline we can either input a
	# concrete `ID`/`NAME` or the option `--name`
	if [ "${COMP_CWORD}" -eq 3 ]; then

		# get possible ids and names
		if [ -d /sys/devices/jailhouse/cells ]; then
			for i in /sys/devices/jailhouse/cells/*/id; do
				ids="${ids} $(cat "${i}" | tr '\n' ' ')"
			done
			for n in /sys/devices/jailhouse/cells/*; do
				_quote_readline_by_ref "${n##*/}" quoted
				names="${names} ${quoted}"
			done
		fi

		COMPREPLY=( $( compgen -W "--name ${ids} ${names}" -- \
					${quoted_cur} ) )

	# if we are already at position 4, may enter a `NAME`, if `--name` was
	# given before
	elif [ "${COMP_CWORD}" -eq 4 ]; then
		[ "${prev}" = "--name" ] || return 1

		# get possible names
		if [ -d /sys/devices/jailhouse/cells ]; then
			for n in /sys/devices/jailhouse/cells/*; do
				_quote_readline_by_ref "${n##*/}" quoted
				names="${names} ${quoted}"
			done
		fi

		COMPREPLY=( $( compgen -W "${names}" -- ${quoted_cur} ) )

	# the id or name is only accepted at position 3 or 4
	else
		return 1;
	fi

	return 0;
}

function _jailhouse_cell_linux() {
	local cur prev word

	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	options="-h --help -i --initrd -c --cmdline -w --write-params"

	# if we already have begun to write an option
	if [[ "$cur" == -* ]]; then
		COMPREPLY=( $( compgen -W "${options}" -- "${cur}") )
	else
		# if the previous was on of the following options
		case "${prev}" in
		-i|--initrd|-w|--write-params)
			# search an existing file
			_filedir
			return $?
			;;
		-c|--cmdline)
			# we can't really predict this
			return 0
			;;
		esac

		# neither option, nor followup of one. Lets assume we want
		# the cell or the kernel
		for n in `seq ${COMP_CWORD-1}`; do
			word="${COMP_WORDS[n]}"
			if [[ "${word}" == *.cell ]] && ( [ $n -eq 1 ] ||
			    [[ "${COMP_WORDS[n-1]}" != -* ]] ); then
				# we already have a cell, this is the kernel
				_filedir
				return 0
			fi
		done
		_filedir "cell"
	fi

	return 0
}

function _jailhouse_cell() {
	local cur prev quoted_cur

	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	_quote_readline_by_ref "$cur" quoted_cur

	# handle subcommand of the cell-command
	case "${1}" in
	create)
		# search for guest-cell configs

		# this command takes only a argument at place 3
		[ "${COMP_CWORD}" -gt 3 ] && return 1

		_filedir "cell"
		;;
	load)
		# first, select the id/name of the cell we want to load a image
		# for
		_jailhouse_get_id "${cur}" "${prev}" && return 0

		# [image & address] can be repeated

		# after id/name insert image-file (always true)
		if [ "${COMP_CWORD}" -eq 4 ] || ( [ "${COMP_CWORD}" -eq 5 ] && \
			[ "${COMP_WORDS[3]}" = "--name" ] ); then

			_filedir
			return 0
		fi

		# the first image has to be placed, after that it is:
		#
		# [image [<-a|--address> <address>]
		#        [image [<-a|--address> <address>] ... ]]

		# prev was an addresse, no image here
		if [[ "${prev}" = "-a" || "${prev}" = "--address" ]]; then
			return 0

		# prev was either an image or a address-number
		else
			# did we already start to type an other switch
			if [[ "$cur" == -* ]]; then
				COMPREPLY=( $( compgen -W "-a --address" -- \
						"${cur}") )
			fi

			# default to image-file
			_filedir
			return 0
		fi

		;;
	start)
		# takes only one argument (id/name)
		_jailhouse_get_id "${cur}" "${prev}" || return 1
		;;
	shutdown)
		# takes only one argument (id/name)
		_jailhouse_get_id "${cur}" "${prev}" || return 1
		;;
	destroy)
		# takes only one argument (id/name)
		_jailhouse_get_id "${cur}" "${prev}" || return 1
		;;
	linux)
		_jailhouse_cell_linux || return 1
		;;
	list)
		# list all cells

		# this command takes only a argument at place 3
		[ "${COMP_CWORD}" -gt 3 ] && return 1

		COMPREPLY=( $( compgen -W "-h --help" -- "${cur}") )
		return 0;;
	stats)
		# takes only one argument (id/name)
		_jailhouse_get_id "${cur}" "${prev}" || return 1

		if [ "${COMP_CWORD}" -eq 3 ]; then
			COMPREPLY=( ${COMPREPLY[@]-} $( compgen -W "-h --help" \
							-- ${quoted_cur} ) )
		fi
		;;
	*)
		return 1;;
	esac

	return 0
}

function _jailhouse_config_create() {
	local cur prev

	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	options="-h --help -g --generate-collector -r --root -t --template-dir \
		--mem-inmates --mem-hv"

	# if we already have begun to write an option
	if [[ "$cur" == -* ]]; then
		COMPREPLY=( $( compgen -W "${options}" -- "${cur}") )
	else
		# if the previous was on of the following options
		case "${prev}" in
		-r|--root|-t|--template-dir)
			# search a existing directory
			_filedir -d
			return $?
			;;
		--mem-inmates|--mem-hv)
			# we can't really predict this
			return 0
			;;
		esac

		# neither option, nor followup of one. Lets assume we want the
		# target-filename
		_filedir
	fi

	return 0
}

function _jailhouse() {
	# returns two value: - numeric from "return" (success/failure)
	#                    - ${COMPREPLY}; an bash-array from which bash will
	#                      read the possible completions (reset this here)
	COMPREPLY=()

	local command command_cell command_config cur prev subcommand

	# first level
	command="enable disable cell config --help"

	# second level
	command_cell="create load start shutdown destroy linux list stats"
	command_config="create collect"

	# ${COMP_WORDS} array containing the words on the current command line
	# ${COMP_CWORD} index into COMP_WORDS, pointing at the current position
	cur="${COMP_WORDS[COMP_CWORD]}"
	prev="${COMP_WORDS[COMP_CWORD-1]}"

	# command line parsing for the jaihouse-tool is pretty static right
	# now, the first two levels can be parsed simpy by comparing
	# postions

	# ${COMP_CWORD} contains at which argument-position we currently are
	#
	# if at level 1, we select from first level list
	if [ "${COMP_CWORD}" -eq 1 ]; then
		COMPREPLY=( $( compgen -W "${command}" -- "${cur}") )

	# if at level 2, we have to evaluate level 1 and look for the main
	# command
	elif [ "${COMP_CWORD}" -eq 2 ]; then
		command="${COMP_WORDS[1]}"

		case "${command}" in
		enable)
			# a root-cell configuration
			_filedir "cell"
			;;
		cell)
			# one of the following subcommands
			COMPREPLY=( $( compgen -W "${command_cell}" -- \
					"${cur}") )
			;;
		config)
			# one of the following subcommands
			COMPREPLY=( $( compgen -W "${command_config}" -- \
					"${cur}") )
			;;
		--help|disable)
			# these first level commands have no further subcommand
			# or option OR we don't even know it
			return 0;;
		*)
			return 1;;
		esac

	# after level 2 it gets more complecated in some cases
	else
		command="${COMP_WORDS[1]}"
		subcommand="${COMP_WORDS[2]}"

		case "${command}" in
		cell)
			# handle cell-commands
			_jailhouse_cell "${subcommand}" || return 1
			;;
		config)
			case "${subcommand}" in
			create)
				_jailhouse_config_create || return 1
				;;
			collect)
				# config-collect writes to a new file

				# this command takes only a argument at place 3
				[ "${COMP_CWORD}" -gt 3 ] && return 1

				_filedir
				;;
			*)
				return 1;;
			esac
			;;
		*)
			# no further subsubcommand/option known for this
			return 1;;
		esac
	fi

	return 0
}
complete -F _jailhouse jailhouse

}
