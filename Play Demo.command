#!/bin/zsh
cd -- "${0:A:h}" || exit 1
python3 tools/ue.py play
result=$?
if (( result != 0 )); then
  print '\nThe demo could not start. Read the message above and README.md.'
  read '?Press Return to close this window.'
fi
exit $result
