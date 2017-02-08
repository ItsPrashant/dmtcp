#ifndef IBVID_H
#define IBVID_H
#include <stdint.h>
#include "lib/list.h"

/*
 * Virtualization of qp_num and lid:
 *
 * In our previous design, to reduce the overhead of virtualization, all IDs
 * are not virtualized before checkpoint, meaning that virtual IDs and real
 * ones are identical. This design has several corner cases that are not
 * covered: duplications of IDs on restart; user caching some of the IDs,
 * whose real values may change on restart.
 *
 * The new design fully virtualizes qp_num and lid when they are created:
 * virtual qp_num is created when the qp is created, and lid is virtualized
 * when the hardware port is queried for the first time. It is ensured that
 * each virtual qp_num and each virtual lid is unique across the computation,
 * so that there will be no conflict from the viewpoint of dmtcp. Virtual
 * qp_num is generated based on virtual pid + an offset, and virtual lid is
 * generated by the cooridnator based on a bi-directional id-to-host mapping.
 *
 * When virtual IDs are created, the virtual-to-real mapping is propagated to
 * the cooridnator, before returning to the user. When it is used (passed in
 * by the application), the plugin is responsible for querying the coordinator,
 * and translate the virtual ID to the real one. On restart, when the resources
 * are recreated, each process needs to send the new mappings to the coordinator.
 *
 * This design can entirely avoid the issues in the original design, but it may
 * have additional overhead: publish/subscribe service requires exchanging small
 * messages between the coordinator and the client. The performance is especially
 * bad when the application scales: suppose there are 10,000 processes, each process
 * has 3 queue pairs and 1 lid, then there are 40,000 messages go through the
 * coordinator. That's why the remote key of memory region still uses the old
 * design.
 *
 * Some new features in 3.0 should improve the performance a lot, such as tree of
 * coordinators, and the coalescing of publish/subscribe messages. One possible
 * way to take of message coalescing is to make the publish/subscribe lazy, and to
 * have some assumptions of the application. For example, MPI implementations tend
 * to have phases: all processes create the queue pairs in one phase, and they
 * exchange the queue pair numbers during the next phase. In this case, the updating
 * of the mapping of all the queue pairs can be delayed until the start of the second
 * phase. Another optimization is, on restart, after all mappings are updated, the
 * coordinator can distribute the entire database to every process. This way, there's
 * no need to query the coordinator after restart any more.
 *
 * */

typedef struct ibv_qp_id {
  uint32_t qpn;
  uint16_t lid;
  uint32_t psn;
} ibv_qp_id_t;

typedef struct {
  uint32_t qpn;
  uint16_t lid;
} ibv_qp_pd_id_t, ibv_ud_qp_id_t;

struct ibv_rkey_id {
  uint32_t pd_id;
  uint32_t rkey;
};

struct ibv_rkey_pair {
  struct ibv_rkey_id orig_rkey;
  uint32_t new_rkey;
  struct list_elem elem;
};

struct ibv_ud_qp_id_pair {
  ibv_ud_qp_id_t orig_id;
  ibv_ud_qp_id_t curr_id;
  struct list_elem elem;
};

typedef struct qp_num_mapping {
  uint32_t virtual_qp_num;
  uint32_t real_qp_num;
  struct list_elem elem;
} qp_num_mapping_t;

ibv_qp_id_t * create_ibv_id(int qpn, int lid, void * buffer, int size);
#endif
