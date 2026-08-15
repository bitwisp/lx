_lx()
{
    local current previous command
    current="${COMP_WORDS[COMP_CWORD]}"
    previous="${COMP_WORDS[COMP_CWORD-1]}"
    command="${COMP_WORDS[1]}"

    case "${previous}" in
        --name|--user|--service|--pid|--lines|--since)
            return
            ;;
    esac

    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W \
            "status process port service svc log inspect find doctor tui --json --quiet --no-color --version --help" \
            -- "${current}") )
        return
    fi

    case "${command}" in
        process)
            COMPREPLY=( $(compgen -W \
                "stop kill --name --user --service --raw-command --json --quiet --no-color --help" \
                -- "${current}") )
            ;;
        port)
            COMPREPLY=( $(compgen -W \
                "free --json --quiet --no-color --yes --help" \
                -- "${current}") )
            ;;
        service|svc)
            COMPREPLY=( $(compgen -W \
                "start stop restart --yes --json --quiet --no-color --help" \
                -- "${current}") )
            ;;
        log)
            COMPREPLY=( $(compgen -W \
                "--pid --lines --since --follow --json --quiet --no-color --help" \
                -- "${current}") )
            ;;
        status|inspect|find|doctor)
            COMPREPLY=( $(compgen -W \
                "--json --quiet --no-color --help" -- "${current}") )
            ;;
        tui)
            COMPREPLY=( $(compgen -W "--help" -- "${current}") )
            ;;
    esac
}

complete -F _lx lx
