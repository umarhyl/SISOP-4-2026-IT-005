#!/usr/bin/env bash
set -euo pipefail

ensure_group() {
  local group="$1"
  if ! getent group "$group" >/dev/null 2>&1; then
    groupadd -r "$group"
  fi
}

ensure_user() {
  local user="$1"
  local group="$2"
  local pass="$3"

  if ! id -u "$user" >/dev/null 2>&1; then
    useradd -M -s /usr/sbin/nologin -g "$group" "$user"
  else
    usermod -g "$group" "$user"
  fi

  echo "$user:$pass" | chpasswd
}

ensure_smb_user() {
  local user="$1"
  local pass="$2"

  if pdbedit -L 2>/dev/null | cut -d: -f1 | grep -qx "$user"; then
    printf "%s\n%s\n" "$pass" "$pass" | smbpasswd -s "$user"
  else
    printf "%s\n%s\n" "$pass" "$pass" | smbpasswd -a -s "$user"
  fi
}

ensure_group readonly
ensure_group staff
ensure_group librarian

ensure_user member readonly member123
ensure_user contributor staff contrib456
ensure_user librarian staff lib789

usermod -aG librarian librarian

ensure_smb_user member member123
ensure_smb_user contributor contrib456
ensure_smb_user librarian lib789

mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs /logs /run/samba

cat > /usr/local/bin/libraryit-preexec.sh <<'SH'
#!/bin/sh
set -e

user="$1"
share="$2"
log_file="/logs/samba_audit.log"

if [ -z "$user" ] || [ -z "$share" ]; then
  exit 0
fi

if [ "$share" = "IPC$" ]; then
  exit 0
fi

ts=$(date +%Y%m%d_%H%M%S)

if [ "$share" = "sourcecode" ]; then
  if ! id -nG "$user" | tr ' ' '\n' | grep -qx staff; then
    printf "%s|%s|%s|connect|fail|%s\n" "$ts" "$user" "$share" "$share" >> "$log_file"
    exit 1
  fi
fi

printf "%s|%s|%s|connect|ok|%s\n" "$ts" "$user" "$share" "$share" >> "$log_file"
exit 0
SH

chmod +x /usr/local/bin/libraryit-preexec.sh

touch /logs/samba_audit.log /logs/libraryit.log

# Normalize permissions for group-based access rules.
chown -R root:staff /libraryit/ebooks /libraryit/papers /libraryit/sourcecode
chmod 2775 /libraryit/ebooks /libraryit/papers
chmod 0750 /libraryit/sourcecode

# Keep docs read-only on host while allowing Samba to gate writes.
chown -R root:staff /libraryit/docs
chmod 0555 /libraryit/docs
setfacl -m g:librarian:rwx /libraryit/docs
setfacl -m d:g:librarian:rwx /libraryit/docs

exec smbd -F --no-process-group
