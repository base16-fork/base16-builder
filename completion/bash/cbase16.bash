#!/usr/bin/env bash

_cbase16_completion() {
	COMPREPLY=($(compgen -W "update build help" "${COMP_WORDS[1]}"))
}

complete -F _cbase16_completion cbase16
