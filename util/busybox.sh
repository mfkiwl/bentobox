#!/bin/bash
mkdir -p base/bin
cd base
for applet in $(./usr/bin/busybox --list); do
  if [ "$applet" != "init" ]; then
    ln -sf /usr/bin/busybox bin/"$applet"
  fi
done
