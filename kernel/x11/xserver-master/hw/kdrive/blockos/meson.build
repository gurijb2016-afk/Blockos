srcs_blockos = [
    'blockos.c',
    'blockos_input.c',
    'blockosinit.c',
    '../../stubs/ddxBeforeReset.c',
]

blockos_server = executable(
    'X',
    srcs_blockos,
    include_directories: [
        inc,
        include_directories('../src'),
    ],
    dependencies: [common_dep],
    link_with: [
        libxserver_main,
        kdrive,
        libxserver_fb,
        libxserver,
        libxserver_config,
        libxserver_xkeyboard_stubs,
        libxserver_xi_stubs,
    ],
    install: true,
)
