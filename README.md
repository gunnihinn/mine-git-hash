# Mine

Mine Git commit hashes in multithreaded C.

## Use

In the top-level of a Git repository, run:

    $ mine-git-hash [OPTION...] PREFIX

Options:

    -h          Print help and exit
    -d          Debug mode; do not make any changes to the Git repo
    -z=ZEROS    Number of leading zeros to look for. Default: 10
    -t=SECONDS  Timeout. Default: 60 seconds

The program will mine Git commit hashes until it either finds one with the
requested number of leading zeros, or until the specified timeout number of
seconds has passed. The run can also be cancelled by sending SIGINT to the
program.

The program shells the actual Git repo manipulations out to its 
[sister program](https://github.com/gunnihinn/git-commit-annotate).

## TODO

- Write tests
- Refactor
- Timeout=0 means run forever
- Multithread
