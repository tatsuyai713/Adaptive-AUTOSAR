# ────────────────────────────────────────────────────────────────────
# ara::com API samples
# Standalone programs that demonstrate ProxyMethod, SkeletonMethod,
# FireAndForget, CommunicationGroup, Field and Event APIs using
# lightweight in-process bindings.  No real transport is required.
# ────────────────────────────────────────────────────────────────────

set(ara_com_sample_names
  01_typed_method_rpc
  02_fire_and_forget
  03_communication_group
  04_field_get_set
  05_event_pubsub
)

foreach(sample_name IN LISTS ara_com_sample_names)
  add_executable(
    sample_${sample_name}
    ${sample_name}.cpp
  )

  target_link_libraries(
    sample_${sample_name}
    PRIVATE
    AdaptiveAutosarAP::ara_core
    AdaptiveAutosarAP::ara_com
  )

  # in_process_bindings.h lives in this directory
  target_include_directories(
    sample_${sample_name}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
endforeach()

# Convenience target that builds all samples at once.
add_custom_target(ara_com_samples)
foreach(sample_name IN LISTS ara_com_sample_names)
  add_dependencies(ara_com_samples sample_${sample_name})
endforeach()
