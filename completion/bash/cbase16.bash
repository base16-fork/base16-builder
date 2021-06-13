#!/usr/bin/env bash

_cbase16_completion() {
	COMPREPLY=($(compgen -W "update build list version help" "${COMP_WORDS[1]}"))
	if [[ ${COMP_WORDS[1]} = "build" ]]; then
		COMPREPLY=($(compgen -W "-c -s -t -o -r" "${COMP_WORDS[2]}"))
	fi
}

complete -F _cbase16_completion cbase16
