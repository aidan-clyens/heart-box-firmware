#!/bin/bash
echo "source /root/.espressif/tools/activate_idf_v5.4.2.sh" >> ~/.bashrc
git config --global --add safe.directory $(pwd)
exec "$@"
