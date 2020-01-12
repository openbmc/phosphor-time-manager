build_rpath: get_option('oe-sdk').enabled() ? rpath : '',

	link_args: dynamic_linker,
	dependencies: [
                gtest,
                gmock,
                sdbusplus,
                phosphor_logging,
        ],
	include_directories: '..'
	),
	workdir: meson.current_source_dir())
