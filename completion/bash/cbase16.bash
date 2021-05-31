#!/usr/bin/env bash

_cbase16_completion() {
	COMPREPLY=($(compgen -W "update build help" "${COMP_WORDS[1]}"))
	if [[ ${COMP_WORDS[1]} = "build" ]]; then
		COMPREPLY=($(compgen -W "-s -t" "${COMP_WORDS[2]}"))
	fi
}

complete -F _cbase16_completion cbase16