#!/usr/bin/env bash

_cbase16_completion() {
	COMPREPLY=($(compgen -W "update build make list version help" "${COMP_WORDS[1]}"))
	if [[ "${COMP_WORDS[1]}" =~ ^(build|make)$ ]]; then
		COMPREPLY=($(compgen -W "-c -s -t -o" "${COMP_WORDS[2]}"))
	fi
}

complete -F _cbase16_completion cbase16
