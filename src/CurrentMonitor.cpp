/**********************************************************************

CurrentMonitor.cpp

**********************************************************************/

#include <Arduino.h>
#include "CurrentMonitor.h"

///////////////////////////////////////////////////////////////////////////////

/*
 Surveillance du courant.
 En cas de surcharge, la voie est coupée et le reste jusqu'au prochain Go de
 Rocrail : plus de remise sous tension automatique, qui faisait un cycle sans fin
 sur un court-circuit persistant et réalimentait la voie après un Stop.
 Pas de printf de flottant ici : la tâche a une petite pile.
*/
void currentMonitorTask(void *p)
{
  // Récupérer le pointeur vers l'instance de CurrentMonitor
  CurrentMonitor *monitor = static_cast<CurrentMonitor *>(p);

  TickType_t xLastWakeTime = xTaskGetTickCount(); // Obtenir l'heure actuelle pour le délai
  bool wasPowered = false;

  for (;;)
  {
    const bool powered = Centrale::power();

    // Remise sous tension : on repart d'une mesure nulle, sinon la moyenne
    // glissante de la surcharge précédente redéclencherait aussitôt.
    if (powered && !wasPowered)
      monitor->m_current = 0;
    wasPowered = powered;

    // Lecture en mV, conversion en mA, lissage
    const float mA = analogReadMilliVolts(monitor->m_pin) * 1000.0f / CURRENT_MV_PER_A;
    monitor->m_current = mA * CURRENT_SAMPLE_SMOOTHING +
                         monitor->m_current * (1.0f - CURRENT_SAMPLE_SMOOTHING);

    // Vérification si le courant dépasse la limite
    if (powered && monitor->m_current > MAX_CURRENT_MA)
    {
      Centrale::setOverload();
      Serial.printf("Surcharge : %ld mA, voie coupee jusqu'au prochain Go\n",
                    (long)monitor->m_current);
      monitor->m_current = 0;
    }

    // Délai pour la prochaine itération
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(CURRENT_SAMPLE_TIME));
  }
}

void currentDisplayTask(void *p)
{
  // Récupérer le pointeur vers l'instance de CurrentMonitor
  CurrentMonitor *monitor = static_cast<CurrentMonitor *>(p);

  for (;;)
  {
    // Afficher la valeur du courant
    Serial.printf("Courant mesure : %ld mA%s\n", (long)monitor->m_current,
                  Centrale::overload() ? " (surcharge)" : "");

    // Attendre une seconde avant la prochaine itération
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
