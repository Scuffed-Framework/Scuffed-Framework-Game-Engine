SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
sh "$SCRIPT_DIR/configure.sh"
sh "$SCRIPT_DIR/build.sh"