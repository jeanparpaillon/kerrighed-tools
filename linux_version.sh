#!/bin/sh

sed '/^vanilla_linux_version/!d; s/.*vanilla_linux_version=//' configure.ac
