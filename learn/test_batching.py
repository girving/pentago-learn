#!/usr/bin/env python3
"""Tests for batch_vmap"""

from batching import batch_vmap
import jax
import jax.numpy as jnp
import numpy as np


def test_batch_vmap():
  def f(key, info):
    o, s, p = info
    y = o + s * jnp.abs(jax.random.normal(key)) ** p
    return y + jax.random.uniform(key, (5,7))

  batch = 17
  sf = jnp.vectorize(f, signature='(2),(3)->(5,7)')
  bf = batch_vmap(batch)(f)
  key = jax.random.PRNGKey(7)
  k = jax.random.split(key, 100)
  i = jax.random.uniform(key, (100,3))
  y = sf(k, i)
  for n in 0, 5, batch, batch + 5, 100:
    assert np.allclose(y[:n], bf(k[:n], i[:n]))


if __name__ == '__main__':
  test_batch_vmap()
