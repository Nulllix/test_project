cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(roadmap_project)

target_sources(app PRIVATE src/main.c)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

foreach(cert_file
  echo-apps-cert.der
  echo-apps-key.der)
    generate_inc_file_for_target(
      app
      src/${cert_file}
      ${gen_dir}/${cert_file}.inc
    )
endforeach()
