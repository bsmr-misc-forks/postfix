/*++
/* NAME
/*	qmgr_peer 3
/* SUMMARY
/*	per-job peers
/* SYNOPSIS
/*	#include "qmgr.h"
/*
/*	QMGR_PEER *qmgr_peer_create(job, queue)
/*	QMGR_JOB *job;
/*	QMGR_QUEUE *queue;
/*
/*	QMGR_PEER *qmgr_peer_find(job, queue)
/*	QMGR_JOB *job;
/*	QMGR_QUEUE *queue;
/*
/*	void qmgr_peer_free(peer)
/*	QMGR_PEER *peer;
/*
/*	QMGR_PEER *qmgr_peer_select(job)
/*	QMGR_JOB *job;
/*
/* DESCRIPTION
/*	These routines add/delete/manipulate per-job peers.
/*	Each queue corresponds to a specific job and destination.
/*      It is similar to per-transport queue structure, but groups
/*      only the entries of the given job.
/*
/*	qmgr_peer_create() creates an empty peer structure for the named
/*	job and destination. It is an error to call this function
/*	if an peer for given combination already exists.
/*
/*	qmgr_peer_find() looks up the peer for the named destination
/*	for the named job. A null result means that the queue
/*	was not found.
/*
/*	qmgr_peer_free() disposes of a per-job queue after all
/*	its entries have been taken care of. It is an error to dispose
/*	of a peer still in use.
/*
/*	qmgr_peer_select() attempts to find a peer of named job that
/*	has messages pending delivery.  This routine implements
/*	round-robin search among job's peers.
/* DIAGNOSTICS
/*	Panic: consistency check failure.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Patrik Rak
/*	patrik@raxoft.cz
/*--*/

/* System library. */

#include <sys_defs.h>

/* Utility library. */

#include <msg.h>
#include <htable.h>
#include <mymalloc.h>

/* Application-specific. */

#include "qmgr.h"

/* qmgr_peer_create - create and initialize message peer structure */

QMGR_PEER *qmgr_peer_create(QMGR_JOB *job, QMGR_QUEUE *queue)
{
    QMGR_PEER *peer;

    peer = (QMGR_PEER *) mymalloc(sizeof(QMGR_PEER));
    peer->queue = queue;
    peer->job = job;
    QMGR_LIST_APPEND(job->peer_list, peer, peers);
    htable_enter(job->peer_byname, queue->name, (char *) peer);
    peer->refcount = 0;
    QMGR_LIST_INIT(peer->entry_list);
    return (peer);
}

/* qmgr_peer_free - release peer structure */

void qmgr_peer_free(QMGR_PEER *peer)
{
    QMGR_JOB *job = peer->job;

    QMGR_LIST_UNLINK(job->peer_list, QMGR_PEER *, peer, peers);
    htable_delete(job->peer_byname, peer->queue->name, (void (*) (char *)) 0) ;
    myfree((char *) peer);
}

/* qmgr_peer_find - lookup peer associated with given job and queue */

QMGR_PEER *qmgr_peer_find(QMGR_JOB *job, QMGR_QUEUE *queue)
{
    return ((QMGR_PEER *) htable_find(job->peer_byname, queue->name));
}

/* qmgr_peer_select - select next peer suitable for delivery within given job */

QMGR_PEER *qmgr_peer_select(QMGR_JOB *job)
{
    QMGR_PEER *peer;
    QMGR_QUEUE *queue;
    
    /*
     * If we find a suitable site, rotate the list to enforce round-robin
     * selection. See similar selection code in qmgr_transport_select().
     */
    for (peer = job->peer_list.next; peer; peer = peer->peers.next) {
        queue = peer->queue;
        if (queue->window > queue->busy_refcount && peer->entry_list.next != 0) {
            QMGR_LIST_ROTATE(job->peer_list, peer, peers);
            if (msg_verbose)
	        msg_info("qmgr_peer_select: %s %s", job->message->queue_id, queue->name);
	    return (peer);
        }
    }
    return (0);
}