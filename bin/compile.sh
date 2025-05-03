#!/usr/bin/env sh

# Navigate into the extension directory
cd ext

# Clean previous build artifacts
make clean

# Run the extconf.rb script to generate the Makefile
ruby extconf.rb

# Build the native extension
make

# Define the base name of the output file (without extension)
OUTPUT_BASE_NAME="audio"

# Define the possible extensions
BUNDLE_EXT=".bundle"
SO_EXT=".so"

# Define the destination directory relative to the original script location
DESTINATION_DIR="../lib/"

# Construct the full paths for the potential output files
BUNDLE_FILE="${OUTPUT_BASE_NAME}${BUNDLE_EXT}"
SO_FILE="${OUTPUT_BASE_NAME}${SO_EXT}"

# Check if the .bundle file exists in the current directory (ext/)
if [ -f "$BUNDLE_FILE" ]; then
  echo "Found '$BUNDLE_FILE'. Moving to '$DESTINATION_DIR'..."
  # Move the .bundle file to the destination directory
  mv "$BUNDLE_FILE" "$DESTINATION_DIR"
  # Check if the move was successful
  if [ $? -eq 0 ]; then
    echo "File moved successfully."
  else
    echo "Error: Failed to move '$BUNDLE_FILE'."
    exit 1 # Exit with an error code if the move fails
  fi

# If the .bundle file doesn't exist, check if the .so file exists
elif [ -f "$SO_FILE" ]; then
  echo "Found '$SO_FILE'. Moving to '$DESTINATION_DIR'..."
  # Move the .so file to the destination directory
  mv "$SO_FILE" "$DESTINATION_DIR"
  # Check if the move was successful
  if [ $? -eq 0 ]; then
    echo "File moved successfully."
  else
    echo "Error: Failed to move '$SO_FILE'."
    exit 1 # Exit with an error code if the move fails
  fi

# If neither file was found
else
  echo "Error: Neither '$BUNDLE_FILE' nor '$SO_FILE' was found after building."
  exit 1 # Exit with an error code if neither file is found
fi

make clean
rm Makefile

# Navigate back to the original directory
cd ..

echo "Build and move script finished."
