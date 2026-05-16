#!/usr/bin/env bash
set -euo pipefail

LIB_ROOT="/libraryit"
LOG_DIR="${LIB_ROOT}/logs"
RAW_LOG="/var/log/libraryit_audit.log"
OUT_LOG="${LOG_DIR}/libraryit.log"

ensure_group() {
  local group="$1"
  if ! getent group "$group" >/dev/null; then
    groupadd "$group"
  fi
}

ensure_user() {
  local user="$1"
  local group="$2"
  if ! id "$user" >/dev/null 2>&1; then
    useradd -M -s /usr/sbin/nologin -g "$group" "$user"
  fi
}

set_smb_pass() {
  local user="$1"
  local pass="$2"
  (echo "$pass"; echo "$pass") | smbpasswd -s -a "$user" >/dev/null
  smbpasswd -e "$user" >/dev/null
}

apply_acls() {
  local path="$1"
  shift
  setfacl -R -b "$path"
  for rule in "$@"; do
    setfacl -R -m "$rule" "$path"
  done
  setfacl -R -m m:rwx "$path"
}

mkdir -p \
  "${LIB_ROOT}/ebooks" \
  "${LIB_ROOT}/papers" \
  "${LIB_ROOT}/sourcecode" \
  "${LIB_ROOT}/docs" \
  "${LOG_DIR}"

touch "$OUT_LOG" "$RAW_LOG"
chmod 0644 "$OUT_LOG"

ensure_group readonly
ensure_group staff
ensure_group docswriter

ensure_user member readonly
ensure_user contributor staff
ensure_user librarian staff
usermod -aG docswriter librarian

set_smb_pass member member123
set_smb_pass contributor contrib456
set_smb_pass librarian lib789

chown -R root:staff "${LIB_ROOT}/ebooks" "${LIB_ROOT}/papers"
chmod 2770 "${LIB_ROOT}/ebooks" "${LIB_ROOT}/papers"
apply_acls "${LIB_ROOT}/ebooks" \
  g:staff:rwx d:g:staff:rwx \
  g:readonly:rx d:g:readonly:rx
apply_acls "${LIB_ROOT}/papers" \
  g:staff:rwx d:g:staff:rwx \
  g:readonly:rx d:g:readonly:rx

chown -R root:staff "${LIB_ROOT}/sourcecode"
chmod 0750 "${LIB_ROOT}/sourcecode"
apply_acls "${LIB_ROOT}/sourcecode" \
  g:staff:rwx d:g:staff:rwx

chown -R root:staff "${LIB_ROOT}/docs"
chmod 0550 "${LIB_ROOT}/docs"
apply_acls "${LIB_ROOT}/docs" \
  g:staff:rx d:g:staff:rx \
  g:readonly:rx d:g:readonly:rx \
  g:docswriter:rwx d:g:docswriter:rwx

cat >/etc/rsyslog.d/libraryit.conf <<'CONF'
module(load="imuxsock")
module(load="imklog")
$template LibraryITRaw,"%msg%\n"
if ($syslogfacility-text == "local7") then {
  action(type="omfile" file="/var/log/libraryit_audit.log" template="LibraryITRaw")
  stop
}
CONF

rsyslogd

tail -n 0 -F "$RAW_LOG" | while read -r line; do
  msg="$line"
  if [[ "$line" == *"smbd_audit:"* ]]; then
    msg="${line#*smbd_audit: }"
  fi

  IFS='|' read -r f1 f2 f3 f4 f5 f6 f7 f8 <<< "$msg"
  action="${f5:-}"
  result="${f6:-}"
  if [[ -z "$action" || -z "$result" ]]; then
    continue
  fi

  user="${f1:-unknown}"
  share="${f2:-}"
  target="${f7:-$share}"
  level="INFO"
  if [[ "$result" != ok* && "$result" != OK* ]]; then
    if echo "$result" | grep -qiE 'fail|error|ACCESS_DENIED|NT_STATUS_ACCESS_DENIED|Permission denied|denied'; then
      level="WARNING"
    else
      continue
    fi
  fi

  case "$share" in
    ebooks|papers|sourcecode|docs) ;;
    *) continue ;;
  esac

  if [[ "$level" == "WARNING" ]]; then
    case "$action" in
      connect|open|openat|create_file|mknod|mkdir|rename|renameat|unlink|unlinkat|rmdir|write|pwrite|pwrite_send) ;;
      *) continue ;;
    esac
    action_out="DENIED"
    target="$share"
  else
    case "$user" in
      member|contributor|librarian) ;;
      *) continue ;;
    esac

    case "$action" in
      connect)
        action_out="CONNECT"
        target="$share"
        ;;
      disconnect)
        action_out="DISCONNECT"
        target="$share"
        ;;
      write|pwrite|pwrite_send)
        action_out="WRITE"
        target="$f7"
        ;;
      *)
        continue
        ;;
    esac

    case "$target" in
      ""|r|w|rw|wr|r+|w+|a|a+|0x*|*:* ) continue ;;
    esac

    target="${target##*/}"
  fi

  ts=$(date '+%Y-%m-%d %H:%M:%S')
  printf '[%s] [%s] [%s] [%s] [%s]\n' "$ts" "$level" "$user" "$action_out" "$target" >> "$OUT_LOG"
done &

tail -n 0 -F /var/log/samba/log.* | while read -r line; do
  case "$line" in
    *"not permitted to access this share ("*)
      user="${line#*user '}"
      user="${user%%'*}"
      share="${line##*(}"
      share="${share%%)*}"
      
      case "$share" in
        ebooks|papers|sourcecode|docs) ;;
        *) continue ;;
      esac
      
      ts=$(date '+%Y-%m-%d %H:%M:%S')
      printf '[%s] [WARNING] [%s] [DENIED] [%s]\n' "$ts" "$user" "$share" >> "$OUT_LOG"
      ;;
  esac
done &

nmbd -D
smbd -F --no-process-group -s /etc/samba/smb.conf
