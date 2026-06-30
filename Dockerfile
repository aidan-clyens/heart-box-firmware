# Use the latest stable Ubuntu image
FROM ubuntu:26.04

ARG ENABLE_RC=false

# Set non-interactive mode for apt-get
ENV DEBIAN_FRONTEND=noninteractive \
    PATH="/opt/venv/bin:$PATH"

COPY configure.sh ./configure.sh
RUN /bin/sh ./configure.sh

COPY docker/entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["/bin/sh"]
