# Tutor mode guard: allow writing .md (teaching docs/memory), deny all other writes.
  input=$(cat)
  path=$(printf '%s' "$input" | sed -n 's/.*"file_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
  case "$path" in
    *.md)
      echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow","permissionDecisionReason":"Tutor mode: markdown docs allowed."}}'
      ;;
    *)
      echo '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"Tutor mode: only .md writes allowed; you write all
  code."}}'
      ;;
  esac