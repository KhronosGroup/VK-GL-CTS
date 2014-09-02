# dEQP Modules
include(glshared/glshared.cmake			OPTIONAL)
include(gles2/gles2.cmake				OPTIONAL)
include(gles3/gles3.cmake				OPTIONAL)
include(gles31/gles31.cmake				OPTIONAL)
include(egl/egl.cmake					OPTIONAL)

# Misc
include(internal/internal.cmake			OPTIONAL)

# Pass DEQP_MODULE_LIBRARIES and DEQP_MODULE_ENTRY_POINTS
set(DEQP_MODULE_LIBRARIES ${DEQP_MODULE_LIBRARIES} PARENT_SCOPE)
set(DEQP_MODULE_ENTRY_POINTS ${DEQP_MODULE_ENTRY_POINTS} PARENT_SCOPE)
