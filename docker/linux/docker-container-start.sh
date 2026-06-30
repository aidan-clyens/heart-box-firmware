#!/usr/bin/env bash

set -euo pipefail

readonly IMAGE="$1"
readonly PLATFORM="linux/amd64"
readonly CONTAINER_NAME="esp-idf-dev"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
readonly CONTAINER_HOME="/home/$(id -un)"

if ! command -v docker >/dev/null 2>&1; then
	echo "docker is required but was not found in PATH." >&2
	exit 1
fi

if docker container inspect "${CONTAINER_NAME}" >/dev/null 2>&1; then
	docker start "${CONTAINER_NAME}" >/dev/null
	echo "Started existing container '${CONTAINER_NAME}'."
	exit 0
fi

docker create -it --init --platform="${PLATFORM}" \
	--name "${CONTAINER_NAME}" \
	-v "${REPO_ROOT}:${CONTAINER_HOME}/${CONTAINER_NAME}" \
	-v "${HOME}/.ssh:/root/.ssh" \
	-v "/tmp/.X11-unix:/tmp/.X11-unix" \
	-v /mnt/wslg/:/mnt/wslg/ \
	-w "${CONTAINER_HOME}/${CONTAINER_NAME}" \
	"${IMAGE}" \
	tail -f /dev/null

docker start "${CONTAINER_NAME}"
docker container inspect "${CONTAINER_NAME}"

echo "Created container '${CONTAINER_NAME}' from ${IMAGE}."
