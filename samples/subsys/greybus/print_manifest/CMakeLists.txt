# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr)
project(greybus-net)

target_sources(app PRIVATE src/main.c)
