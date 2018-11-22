#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ev3.h>
#include <ev3_light.h>
#include <ev3_port.h>
#include <ev3_tacho.h>

#define MULTI_SET_TACHO_COMMAND_INX(sn,c) do {				\
    _bytes = multi_set_tacho_command_inx((sn), (c));			\
    if (_bytes == 0) {							\
      zlog_error(zlog_c,						\
		 "Impossible d'envoyer la commande '%d' aux servomoteurs", \
		 (c));							\
      ev3_uninit();							\
      return 0;								\
    }									\
  } while(0);

#define MULTI_SET_TACHO_DUTY_CYCLE_SP(sn,v) do {			\
    _bytes = multi_set_tacho_duty_cycle_sp((sn), (v));			\
    if (_bytes == 0) {							\
      zlog_warn(zlog_c,							\
		"Impossible de changer le rapport cyclique à '%d' pour le grand servomoteurs", \
		(v));							\
    }									\
  } while(0);

#define MULTI_SET_TACHO_SPEED_SP(sn,v) do {				\
    _bytes = multi_set_tacho_speed_sp((sn), (v));			\
    if (_bytes == 0) {							\
      zlog_warn(zlog_c,							\
		"Impossible de changer la vitesse à '%d' pour les servomoteurs", \
		(v));							\
      ev3_uninit();							\
      return 0;								\
    }									\
  } while(0);

#define MULTI_SET_TACHO_STOP_ACTION_INX(sn,v) do {			\
    _bytes = multi_set_tacho_stop_action_inx((sn), (v));		\
    if (_bytes == 0) {							\
      zlog_warn(zlog_c,							\
		"Impossible d'assigner l'action '%d' aux servomoteurs",	\
		(v));							\
      ev3_uninit();							\
      return 0;								\
    }									\
  } while(0);

#define MIN(a,b) ((a) < (b) ? (a) : (b))

// Numéro de séquence des servomoteurs
#define TACHO_LEFT_SN tacho_sn[0]
#define TACHO_RIGHT_SN tacho_sn[1]

// Drapeau pour verifier la présence des capteurs
#define HAVE_TACHO_LEFT 0b1
#define HAVE_TACHO_RIGHT 0b10

// Variable globale spécifique à zlog
zlog_category_t *zlog_c;

// Tableau pour les numéros de séquence des capteurs
uint8_t tacho_sn[TACHO_DESC__LIMIT_];

// Variable globale pour les macros
size_t _bytes;

/*
 * Vitesse maximale de servomoteurs. La vitesse maximale est assigné durant
 * l'initialisation. La valeur standard est 0.
 */
int max_spd = 0;

int init(void) {
  int i, rc, max_spd_left = 0, max_spd_right = 0;
  size_t bytes;
  unsigned char tachos = 0;
  uint8_t sn;

  for (i = 0; i < TACHO_DESC__LIMIT_; i++)
    tacho_sn[i] = DESC_LIMIT;

  // Initialisation de la brique intelligente EV3
  rc = ev3_init();
  if (rc == 1)
    zlog_info(zlog_c, "Brique intelligente EV3 trouvée");
  else {
    if (rc == 0)
      zlog_fatal(zlog_c, "Brique intelligente EV3 pas trouvée");
    else
      zlog_error(zlog_c, "ev3_init retourne erreur '%d'", rc);
    return rc;
  }
  // Initialisation des ports EV3
  rc = ev3_port_init();
  if (rc == -1) {
    zlog_error(zlog_c, "Erreur durant l'initialisation des ports EV3");
    ev3_uninit();
    return rc;
  } else if (rc != 8)
    zlog_warn(zlog_c, "'%d/8' ports EV3 retrouvé");
  else
    zlog_info(zlog_c, "Tous ports EV3 retrouvé");
  // Initialisation des servomoteurs
  rc = ev3_tacho_init();
  if (rc == -1) {
    zlog_error(zlog_c, "Erreur durant l'initialisation des servomoteurs EV3");
    ev3_uninit();
    return rc;
  } else
    zlog_info(zlog_c, "'%d' servomoteurs EV3 retrouvé", rc);
  /*
   * Assurer que les servomoteurs sont mis dans la brique et mettre en
   * correspondance les numéros de séquence.
   */
  for (i = 0; i < TACHO_DESC__LIMIT_; i++) {
    if (ev3_search_tacho_plugged_in(ev3_tacho[i].port, ev3_tacho[i].extport,
				    &sn, 0)) {
      if (ev3_tacho[i].type_inx == LEGO_EV3_L_MOTOR) {
	if (ev3_tacho[i].port == OUTPUT_A) {
	  tachos |= HAVE_TACHO_LEFT;
	  TACHO_LEFT_SN = i;
	  bytes = get_tacho_max_speed(i, &max_spd_left);
	} else {
	  tachos |= HAVE_TACHO_RIGHT;
	  TACHO_RIGHT_SN = i;
	  bytes = get_tacho_max_speed(i, &max_spd_right);
	}
      }
      if (bytes == 0) {
	zlog_error(zlog_c, "Impossible de lire la vitesse maximale pour '%s'",
		   ev3_tacho_type(ev3_tacho[i].type_inx));
	ev3_uninit();
	return 0;
      }
    } else {
      zlog_error(zlog_c, "Un des grand servomoteur n'est pas connecté à la brique intelligente EV3");
      ev3_uninit();
      return 0;
    }
  }
  if (tachos != (HAVE_TACHO_LEFT | HAVE_TACHO_RIGHT)) {
    zlog_fatal(zlog_c, "Les grand servomoteurs n'ont pas été retrouvé");
    ev3_uninit();
    return 0;
  }
  /*
   * Détermination de la vitesse maximale des deux grand servomoteurs. La
   * vitesse maximale correspond à la plus petite vitesse d'un des deux
   * servomoteurs.
   */
  bytes = multi_set_tacho_command_inx(tacho_sn, TACHO_RESET);
  if (bytes == 0) {
    zlog_error(zlog_c, "Impossible d'envoyer la commande 'TACHO_RESET' aux servomoteurs");
    ev3_uninit();
    return 0;
  }
  max_spd = MIN(max_spd_left, max_spd_right);
  /*
   * Indication aux servomoteurs d'interpréter les valeurs positives comme
   * mouvement en avant.
   */
  bytes = multi_set_tacho_polarity_inx(tacho_sn, TACHO_NORMAL);
  if (bytes == 0) {
    zlog_error(zlog_c, "Impossible de changer la polarité 'TACHO_NORMAL' pour les servomoteurs");
    ev3_uninit();
    return 0;
  }
  bytes = multi_set_tacho_duty_cycle_sp(tacho_sn, 0);
  if (bytes == 0) {
    zlog_error(zlog_c, "Impossible de changer le rapport cyclique '%d' pour les servomoteurs", 0);
    ev3_uninit();
    return 0;
  }

  return 1;
}

int main (int argc, char *argv[]) {
  int condition = 0, color_idx, rc;

  // zlog specific variables
  const char *zlog_conf = "/etc/zlog.conf";
  const char *zlog_cat  = "project";

  rc = zlog_init(zlog_conf);
  if (rc) {
    printf("zlog initialization using '%s' failed\n", zlog_conf);
    return EXIT_FAILURE;
  }

  zlog_c = zlog_get_category(zlog_cat);
  if (!zlog_c) {
    printf("zlog unable to retrieve category '%s'\n", zlog_cat);
    puts("zlog logging disabled");
    zlog_fini();
  }
  
  zlog_info(zlog_c, "Hello IIUN!");

  if(!init()) {
    zlog_fini();
    return EXIT_FAILURE;
  }

  zlog_info(zlog_c, "Arrêter les servomoteurs");
  // A compléter
  
  ev3_uninit();
    
  zlog_info(zlog_c, "Bye IIUN!");

  zlog_fini();

  return EXIT_SUCCESS;
}
