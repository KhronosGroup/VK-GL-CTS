
macro(MAKE_LCOV_TARGET name)
 add_custom_target(${name}-lcov lcov -d ${PROJECT_BINARY_DIR}/${name}/CMakeFiles/${name}.dir -o ${name}.info -c > /dev/null WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/${name} COMMENT "Running lcov for ${name}" VERBATIM)
endmacro()

macro(MAKE_LCOV_REPORT_TARGET target exe-targets projects outdir title)
  foreach (PRJ ${projects})
	MAKE_LCOV_TARGET(${PRJ})
	set(INFO_FILES ${PRJ}/${PRJ}.info ${INFO_FILES})
	add_dependencies(${PRJ}-lcov ${exe-targets})
	set(LCOV_PRJS ${LCOV_PRJS} ${PRJ}-lcov)
  endforeach()
  add_custom_target(${target} ALL genhtml -o ${outdir} -t "${title}" ${INFO_FILES} | grep "\\.\\.:" WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" COMMENT "Generate LCOV HTML report" VERBATIM)
  add_dependencies(${target} ${LCOV_PRJS})

endmacro()
