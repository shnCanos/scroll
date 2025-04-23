# scroll(1) completion

complete -f -c scroll
complete -c scroll -s h -l help --description "Show help message and quit."
complete -c scroll -s c -l config --description "Specifies a config file." -r
complete -c scroll -s C -l validate --description "Check the validity of the config file, then exit."
complete -c scroll -s d -l debug --description "Enables full logging, including debug information."
complete -c scroll -s v -l version --description "Show the version number and quit."
complete -c scroll -s V -l verbose --description "Enables more verbose logging."
complete -c scroll -l get-socketpath --description "Gets the IPC socket path and prints it, then exits."

