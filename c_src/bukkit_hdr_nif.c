#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "erl_nif.h"
#include "bukkit_hdr.h"

typedef struct
{
  Hdr *hdr;
} HdrWrap;

typedef struct
{
  ERL_NIF_TERM atom_error;
  ERL_NIF_TERM atom_ok;
  ERL_NIF_TERM atom_undefined;
  ErlNifResourceType *res_hdr;
} HdrPriv;

static ERL_NIF_TERM make_atom(ErlNifEnv* env, const char* name)
{
  ERL_NIF_TERM ret;
  if (enif_make_existing_atom(env, name, &ret, ERL_NIF_LATIN1)) {
    return ret;
  }

  return enif_make_atom(env, name);
}

static ERL_NIF_TERM
bukkit_hdr_nif_new(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  HdrWrap *wrap = NULL;
  HdrPriv *priv = enif_priv_data(env);
  ErlNifSInt64 lowest, highest;
  int sigfig;

  if (argc != 3) {
    return enif_make_badarg(env);
  }

  if (!enif_get_int64(env, argv[0], &lowest)) {
    return enif_make_badarg(env);
  }

  if (!enif_get_int64(env, argv[1], &highest)) {
    return enif_make_badarg(env);
  }

  if (!enif_get_int(env, argv[2], &sigfig)) {
    return enif_make_badarg(env);
  }

  if (lowest < 1 || sigfig < 1 || 5 < sigfig || lowest > highest) {
    return enif_make_badarg(env);
  }

  wrap = (HdrWrap*) enif_alloc_resource(priv->res_hdr, sizeof(HdrWrap));
  if (wrap == NULL) {
    return priv->atom_undefined;
  }

  memset(wrap, '\0', sizeof(HdrWrap));
  if (bukkit_hdr_new(lowest, highest, sigfig, &wrap->hdr) != 0) {
    return priv->atom_undefined;
  }

  ERL_NIF_TERM ret = enif_make_resource(env, wrap);
  enif_release_resource(wrap);
  return enif_make_tuple2(env, priv->atom_ok, ret);
}

static ERL_NIF_TERM
bukkit_hdr_nif_update(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  HdrWrap *wrap = NULL;
  HdrPriv *priv = enif_priv_data(env);
  ErlNifSInt64 value;

  if (argc != 2) {
    return enif_make_badarg(env);
  }

  if (!enif_get_resource(env, argv[0], priv->res_hdr, (void**) &wrap)) {
    return enif_make_badarg(env);
  }

  if (!enif_get_int64(env, argv[1], &value)) {
    return enif_make_badarg(env);
  }

  if (bukkit_hdr_update(wrap->hdr, value) != 0) {
    return priv->atom_error;
  }

  return priv->atom_ok;
}

static ERL_NIF_TERM
bukkit_hdr_nif_read_int(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  ERL_NIF_TERM ret;
  HdrWrap **wraps = NULL;
  HdrRead *read = NULL;
  Hdr *agg = NULL;
  HdrPriv *priv = enif_priv_data(env);

  if (argc != 1) {
    ret = enif_make_badarg(env);
    goto done;
  }

  unsigned int n;
  if (!enif_get_list_length(env, argv[0], &n)) {
    ret = enif_make_badarg(env);
    goto done;
  }

  wraps = calloc(n, sizeof(HdrWrap*));
  if (wraps == NULL) {
    ret = priv->atom_undefined;
    goto done;
  }

  ERL_NIF_TERM head;
  ERL_NIF_TERM tail = argv[0];
  for (int i = 0; i < n; i++) {
    if (!enif_get_list_cell(env, tail, &head, &tail)) {
      ret = enif_make_badarg(env);
      goto done;
    }

    if (!enif_get_resource(env, head, priv->res_hdr, (void**) &wraps[i])) {
      ret = enif_make_badarg(env);
      goto done;
    }
  }

  int64_t lowest = wraps[0]->hdr->lowest_trackable_value;
  int64_t highest = wraps[0]->hdr->highest_trackable_value;
  int sigfig = wraps[0]->hdr->significant_figures;
  if (bukkit_hdr_new(lowest, highest, sigfig, &agg) != 0) {
    ret = priv->atom_error;
    goto done;
  }

  for (int i = 0; i < n; i++) {
    if (wraps[i]->hdr->lowest_trackable_value != lowest) {
      ret = enif_make_badarg(env);
      goto done;
    }

    if (wraps[i]->hdr->highest_trackable_value != highest) {
      ret = enif_make_badarg(env);
      goto done;
    }

    if (wraps[i]->hdr->significant_figures != sigfig) {
      ret = enif_make_badarg(env);
      goto done;
    }

    if (bukkit_hdr_add(wraps[i]->hdr, agg) != 0) {
      ret = priv->atom_error;
      goto done;
    }
  }

  if (bukkit_hdr_read(agg, &read) != 0) {
    ret = priv->atom_error;
    goto done;
  }

  ERL_NIF_TERM undef = priv->atom_undefined;
  ret = enif_make_tuple9(
    env,
    read->min == INT64_MAX ? undef : enif_make_int64(env, read->min),
    read->max == 0 ? undef : enif_make_int64(env, read->max),
    read->p50 == 0 ? undef : enif_make_int64(env, read->p50),
    read->p75 == 0 ? undef : enif_make_int64(env, read->p75),
    read->p90 == 0 ? undef : enif_make_int64(env, read->p90),
    read->p99 == 0 ? undef : enif_make_int64(env, read->p99),
    read->p999 == 0 ? undef : enif_make_int64(env, read->p999),
    // NAN == NAN is false
    read->mean != read->mean ? undef : enif_make_double(env, read->mean),
    read->stddev != read->stddev ? undef : enif_make_double(env, read->stddev)
  );

 done:
  if (wraps != NULL) {
    free(wraps);
  }

  if (read != NULL) {
    bukkit_hdr_read_free(read);
  }

  if (agg != NULL) {
    bukkit_hdr_free(agg);
  }

  return ret;
}


static void
bukkit_hdr_nif_destroy(ErlNifEnv* env, void* obj)
{
  HdrWrap *wrap = (HdrWrap *) obj;
  if (wrap != NULL) {
    bukkit_hdr_free(wrap->hdr);
  }

  return;
}

static int
load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info)
{
  ErlNifResourceType* res;

  HdrPriv* new_priv = (HdrPriv*)enif_alloc(sizeof(HdrPriv));
  if (new_priv == NULL) {
    return 1;
  }

  res = enif_open_resource_type(
    env,
    NULL,
    "bukkit_hdr",
    bukkit_hdr_nif_destroy,
    ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER,
    NULL
  );

  if (res == NULL) {
    return 1;
  }

  new_priv->atom_error = make_atom(env, "error");
  new_priv->atom_ok = make_atom(env, "ok");
  new_priv->atom_undefined = make_atom(env, "undefined");
  new_priv->res_hdr = res;
  *priv = (void*)new_priv;
  return 0;
}

static void
unload(ErlNifEnv* env, void* priv)
{
  enif_free(priv);
  return;
}

static int
upgrade(ErlNifEnv* env, void** priv, void** old_priv, ERL_NIF_TERM load_info)
{
  *priv = *old_priv;
  return 0;
}

static ErlNifFunc funcs[] = {
  {"new", 3, bukkit_hdr_nif_new},
  {"update", 2, bukkit_hdr_nif_update},
  {"read_int", 1, bukkit_hdr_nif_read_int}
};

ERL_NIF_INIT(bukkit_hdr, funcs, &load, NULL, &upgrade, &unload);
