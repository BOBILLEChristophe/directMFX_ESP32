/**********************************************************************

CurrentMonitor.cpp

**********************************************************************/

#include <Arduino.h>
#include "CurrentMonitor.h"

///////////////////////////////////////////////////////////////////////////////

void currentMonitorTask(void *p)
{
  // Récupérer le pointeur vers l'instance de CurrentMonitor
  CurrentMonitor *monitor = static_cast<CurrentMonitor *>(p);

  TickType_t xLastWakeTime = xTaskGetTickCount(); // Obtenir l'heure actuelle pour le délai

  for (;;)
  {
    // Lecture de la valeur de courant et calcul du courant lissé
    monitor->m_current = analogRead(monitor->m_pin) * CURRENT_SAMPLE_SMOOTHING +
                         monitor->m_current * (1.0 - CURRENT_SAMPLE_SMOOTHING);

    // Vérification si le courant dépasse la limite
    if (monitor->m_current > MAX_CURRENT)
    {
      Centrale::setPower(false);
      Serial.printf("Current limit exceeded : %.2fmA\n", monitor->m_current);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      Centrale::setPower(true);
      monitor->m_current = 0;
    }

    // Délai pour la prochaine itération
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
  }
}

void currentDisplayTask(void *p)
{
    // Récupérer le pointeur vers l'instance de CurrentMonitor
    CurrentMonitor *monitor = static_cast<CurrentMonitor *>(p);

    for (;;)
    {
        // Afficher la valeur du courant
        Serial.printf("Current measured: %.2f mA\n", monitor->m_current);

        // Attendre une seconde avant la prochaine itération
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

