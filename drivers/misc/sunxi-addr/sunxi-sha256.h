/*
 * Local implement of sha256.
 *
 * Copyright (C) 2013 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef __SUNXI_SHA256_H
#define __SUNXI_SHA256_H

extern int hmac_sha256(const uint8_t *plaintext, ssize_t psize, uint8_t *output);

#endif  /* __SUNXI_SHA256_H */
