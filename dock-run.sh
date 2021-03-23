#!/bin/bash -ex

tag=$(echo ${PWD} | tr / - | cut -b2- | tr A-Z a-z)
groups=$(id -G | xargs -n1 echo -n " --group-add ")
params="-v ${PWD}:${PWD} --rm -w ${PWD} -u"$(id -u):$(id -g)" $groups -v/etc/passwd:/etc/passwd -v/etc/group:/etc/group"


# If SSH Agent is detected, mount the necessary files and set the SSH_AUTH_SOCK variable
[ -e "${SSH_AUTH_SOCK}" ] && {
    params+=" -v/etc/ssl:/etc/ssl"
    params+=" -v$(readlink -f ${SSH_AUTH_SOCK}):/ssh-agent -eSSH_AUTH_SOCK=/ssh-agent"
    params+=" -v${HOME}/.ssh/known_hosts:${HOME}/.ssh/known_hosts"
}

# Append the tag after all options, but before the command
params+=" ${tag}"

docker build --tag=${tag} docker

docker run $params "$@"
