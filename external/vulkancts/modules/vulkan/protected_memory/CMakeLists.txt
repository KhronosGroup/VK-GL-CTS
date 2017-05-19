include_directories(..)

set(DEQP_VK_PROTECTED_MEMORY_SRCS
	vktProtectedMemContext.hpp
	vktProtectedMemUtils.cpp
	vktProtectedMemUtils.hpp
	vktProtectedMemImageValidator.cpp
	vktProtectedMemImageValidator.hpp
	vktProtectedMemAttachmentLoadTests.cpp
	vktProtectedMemAttachmentLoadTests.hpp
	vktProtectedMemTests.cpp
	vktProtectedMemTests.hpp
	)

set(DEQP_VK_PROTECTED_MEMORY_LIBS
	tcutil
	vkutil
	)

add_library(deqp-vk-protected-memory STATIC ${DEQP_VK_PROTECTED_MEMORY_SRCS})
target_link_libraries(deqp-vk-protected-memory ${DEQP_VK_PROTECTED_MEMORY_LIBS})
