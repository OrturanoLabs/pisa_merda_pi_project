#!/bin/bash

# Compila usando il Makefile
if make; then
    ./pisa_merda_pi_project
else
    echo "Errore durante la compilazione."
    exit 1
fi
