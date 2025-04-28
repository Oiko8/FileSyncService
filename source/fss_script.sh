#!/bin/bash

declare -A syncMap
declare -A statusMap
declare -A timeMap

# checks the args
while getopts ":p:c:" opt; do
  case $opt in
    p)
      path="$OPTARG"
      ;;
    c)
      command="$OPTARG"
      ;;
    \?)
      echo "Not available choice: -$OPTARG" >&2
      exit 1
      ;;
    :)
      echo "The choice -$OPTARG needs an argument." >&2
      exit 1
      ;;
  esac
done

# check if a path is given
if [ -z "$path" ]; then
  echo "You need to give a path with the -p"
  exit 1
fi


# ---------- Implement "purge" command -------------- #
if [ "$command" == "purge" ]; then
  if [ -d "$path" ]; then
    echo "Deleting $path..."
    rm -rf "${path:?}/"*
    echo "Purge complete."
  elif [ -f "$path" ]; then
    echo "Deleting $path..."
    > "$path"
    echo "Purge Complete."
  else
    echo "Logfile/Directory is not correct."
    exit 1
  fi

# -------- implement listAll command ------------- #
elif [ "$command" == "listAll" ]; then
  if [ ! -f "$path" ]; then
    echo "You need a log file."
    exit 1
  fi

  while IFS= read -r line; do
    # take each field in the bracket []. Assuming the manager log has the appropriate format
    timestamp=$(echo "$line" | grep -oP '\[\K[^\]]+' | sed -n '1p')
    source=$(echo "$line" | grep -oP '\[\K[^\]]+' | sed -n '2p')
    dest=$(echo "$line" | grep -oP '\[\K[^\]]+' | sed -n '3p')
    status=$(echo "$line" | grep -oP '\[\K[^\]]+' | sed -n '6p')

    key="${source}->${dest}"

    # We keep the latest change of each source directory
    prev_time="${timeMap[$key]}"
    if [[ -z "$prev_time" || "$timestamp" > "$prev_time" ]]; then
      timeMap["$key"]="$timestamp"
      statusMap["$key"]="$status"
    fi
  done < "$path"

  # Print the results 
  for key in "${!timeMap[@]}"; do
    src=$(echo "$key" | cut -d'>' -f1)
    dst=$(echo "$key" | cut -d'>' -f2 | sed 's/^/ /')
    echo "$src->$dst [Last Sync: ${timeMap[$key]}] [${statusMap[$key]}]"
  done

else
  echo "Uknown command: $command"
  exit 1
fi