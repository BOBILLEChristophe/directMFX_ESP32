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
    // Serial.println(monitor->m_current);
    if (monitor->m_current > CURRENT_SAMPLE_MAX)
    {
      Centrale::setPower(false);
      Serial.println("Current limit exceeded.");
      Serial.println(monitor->m_current);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      Centrale::setPower(true);
      monitor->m_current = 0;
    }

    // Délai pour la prochaine itération
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
  }
}
