#!/bin/bash
echo "source /root/.espressif/tools/activate_idf_v5.4.2.sh" >> ~/.bashrc
git config --global --add safe.directory $(pwd)
git submodule sync --recursive
git -c protocol.version=2 submodule update --init --force --depth=1 --recursive
exec "$@"
