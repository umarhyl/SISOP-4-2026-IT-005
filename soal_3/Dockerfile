FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends samba samba-common-bin samba-vfs-modules smbclient acl \
    && rm -rf /var/lib/apt/lists/*

COPY smb.conf /etc/samba/smb.conf
COPY entrypoint.sh /entrypoint.sh

RUN chmod +x /entrypoint.sh

EXPOSE 445

ENTRYPOINT ["/entrypoint.sh"]
