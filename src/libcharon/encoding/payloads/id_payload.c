/*
 * Copyright (C) 2005-2010 Martin Willi
 * Copyright (C) 2010 revosec AG
 * Copyright (C) 2007 Tobias Brunner
 * Copyright (C) 2005 Jan Hutter
 *
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <stddef.h>

#include "id_payload.h"

#include <daemon.h>
#include <encoding/payloads/encodings.h>

typedef struct private_id_payload_t private_id_payload_t;

/**
 * Private data of an id_payload_t object.
 */
struct private_id_payload_t {

	/**
	 * Public id_payload_t interface.
	 */
	id_payload_t public;

	/**
	 * Next payload type.
	 */
	u_int8_t next_payload;

	/**
	 * Critical flag.
	 */
	bool critical;

	/**
	 * Reserved bits
	 */
	bool reserved_bit[7];

	/**
	 * Reserved bytes
	 */
	u_int8_t reserved_byte[3];

	/**
	 * Length of this payload.
	 */
	u_int16_t payload_length;

	/**
	 * Type of the ID Data.
	 */
	u_int8_t id_type;

	/**
	 * The contained id data value.
	 */
	chunk_t id_data;

	/**
	 * Tunneled protocol ID for IKEv1 quick modes.
	 */
	u_int8_t protocol_id;

	/**
	 * Tunneled port for IKEv1 quick modes.
	 */
	u_int16_t port;

	/**
	 * one of ID_INITIATOR, ID_RESPONDER and IDv1
	 */
	payload_type_t type;
};

/**
 * Encoding rules for an IKEv2 ID payload
 */
static encoding_rule_t encodings_v2[] = {
	/* 1 Byte next payload type, stored in the field next_payload */
	{ U_INT_8,			offsetof(private_id_payload_t, next_payload) 	},
	/* the critical bit */
	{ FLAG,				offsetof(private_id_payload_t, critical) 		},
	/* 7 Bit reserved bits */
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[0])	},
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[1])	},
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[2])	},
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[3])	},
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[4])	},
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[5])	},
	{ RESERVED_BIT,		offsetof(private_id_payload_t, reserved_bit[6])	},
	/* Length of the whole payload*/
	{ PAYLOAD_LENGTH,	offsetof(private_id_payload_t, payload_length) 	},
	/* 1 Byte ID type*/
	{ U_INT_8,			offsetof(private_id_payload_t, id_type)			},
	/* 3 reserved bytes */
	{ RESERVED_BYTE,	offsetof(private_id_payload_t, reserved_byte[0])},
	{ RESERVED_BYTE,	offsetof(private_id_payload_t, reserved_byte[1])},
	{ RESERVED_BYTE,	offsetof(private_id_payload_t, reserved_byte[2])},
	/* some id data bytes, length is defined in PAYLOAD_LENGTH */
	{ CHUNK_DATA,		offsetof(private_id_payload_t, id_data)			},
};

/*
                           1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      ! Next Payload  !C!  RESERVED   !         Payload Length        !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !   ID Type     !                 RESERVED                      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                                                               !
      ~                   Identification Data                         ~
      !                                                               !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

/**
 * Encoding rules for an IKEv1 ID payload
 */
static encoding_rule_t encodings_v1[] = {
	/* 1 Byte next payload type, stored in the field next_payload */
	{ U_INT_8,			offsetof(private_id_payload_t, next_payload)	},
	/* Reserved Byte is skipped */
	{ RESERVED_BYTE,	offsetof(private_id_payload_t, reserved_byte[0])},
	/* Length of the whole payload*/
	{ PAYLOAD_LENGTH,	offsetof(private_id_payload_t, payload_length)	},
	/* 1 Byte ID type*/
	{ U_INT_8,			offsetof(private_id_payload_t, id_type)			},
	{ U_INT_8,			offsetof(private_id_payload_t, protocol_id)		},
	{ U_INT_16,			offsetof(private_id_payload_t, port)			},
	/* some id data bytes, length is defined in PAYLOAD_LENGTH */
	{ CHUNK_DATA,		offsetof(private_id_payload_t, id_data)			},
};

/*
                           1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      ! Next Payload  !    RESERVED   !         Payload Length        !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !   ID Type     ! Protocol ID   !           Port                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      !                                                               !
      ~                   Identification Data                         ~
      !                                                               !
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/


METHOD(payload_t, verify, status_t,
	private_id_payload_t *this)
{
	if (this->id_type == 0 || this->id_type == 4)
	{
		/* reserved IDs */
		DBG1(DBG_ENC, "received ID with reserved type %d", this->id_type);
		return FAILED;
	}
	return SUCCESS;
}

METHOD(payload_t, get_encoding_rules, int,
	private_id_payload_t *this, encoding_rule_t **rules)
{
	if (this->type == ID_V1)
	{
		*rules = encodings_v1;
		return countof(encodings_v1);
	}
	*rules = encodings_v2;
	return countof(encodings_v2);
}

METHOD(payload_t, get_header_length, int,
	private_id_payload_t *this)
{
	return 8;
}

METHOD(payload_t, get_type, payload_type_t,
	private_id_payload_t *this)
{
	return this->type;
}

METHOD(payload_t, get_next_type, payload_type_t,
	private_id_payload_t *this)
{
	return this->next_payload;
}

METHOD(payload_t, set_next_type, void,
	private_id_payload_t *this, payload_type_t type)
{
	this->next_payload = type;
}

METHOD(payload_t, get_length, size_t,
	private_id_payload_t *this)
{
	return this->payload_length;
}

METHOD(id_payload_t, get_identification, identification_t*,
	private_id_payload_t *this)
{
	return identification_create_from_encoding(this->id_type, this->id_data);
}

METHOD2(payload_t, id_payload_t, destroy, void,
	private_id_payload_t *this)
{
	free(this->id_data.ptr);
	free(this);
}

/*
 * Described in header.
 */
id_payload_t *id_payload_create(payload_type_t type)
{
	private_id_payload_t *this;

	INIT(this,
		.public = {
			.payload_interface = {
				.verify = _verify,
				.get_encoding_rules = _get_encoding_rules,
				.get_header_length = _get_header_length,
				.get_length = _get_length,
				.get_next_type = _get_next_type,
				.set_next_type = _set_next_type,
				.get_type = _get_type,
				.destroy = _destroy,
			},
			.get_identification = _get_identification,
			.destroy = _destroy,
		},
		.next_payload = NO_PAYLOAD,
		.payload_length = get_header_length(this),
		.type = type,
	);
	return &this->public;
}

/*
 * Described in header.
 */
id_payload_t *id_payload_create_from_identification(payload_type_t type,
													identification_t *id)
{
	private_id_payload_t *this;

	this = (private_id_payload_t*)id_payload_create(type);
	this->id_data = chunk_clone(id->get_encoding(id));
	this->id_type = id->get_type(id);
	this->payload_length += this->id_data.len;

	return &this->public;
}
